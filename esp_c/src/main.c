#include "mgos.h"
#include "mgos_arduino_onewire.h"
#include "mgos_arduino_dallas_temp.h"
#include "mgos_timers.h"
#include "mgos_gpio.h"
#include "mgos_rpc.h"
#include "mgos_net.h"
#include "mgos_shadow.h"
#include "mgos_dash.h"
#include "mgos_wifi.h"
#include "mgos_config.h"
#include "mgos_system.h"


// 12 -> D6  DS18b20
// 13 -> D7  light
// 2  -> D4  int led
 
// 14 -> D5  Вентилятор
// 0  -> D3  в момент старта должны быть подтянуты к единичке    provision
// 15 -> D8  WDDonePin  в момент старта должен быть подтянут к нулю

// 4,5,12,13,14 - пины без всяких условностей.
// 0 и 2 - в момент старта должны быть подтянуты к единичке. Это ничуть не мешает использовать их, к примеру, для той же I2C шины, где заодно нужна подтяжка.
// 15 - в момент старта должен быть подтянут к нулю. Это более сложная ситуация. Чтобы не заморачиваться - можно использовать его в качестве выхода.
// 16 - выход с открытым коллектором. Лучше не использовать, резервируя до времен когда он понадобится для выхода из спящего режима.
// 1,3 - UART - лучше не трогать. Используется для программирования и общения с компом или другими устройствами. В Nodemcu он уже подключен к микросхеме интерфейса. лучше и оставить для этих целей, IMHO.

// подключение датчиков. маркировка на себя слева направо

// Новая логика
// Если  дельта т больше порога, запомнить t2, включить  вентилятор  на 5 мин и не выключать  до тех пор, пока т1 не опустится  ниже т2+0.5

// черно-син, бел, син-черный
// зелен-бел оран белозеленый
// опрос всех дачиков

// https://github.com/mongoose-os-libs/mjs/blob/master/fs/api_http.js
// https://mongoose-os.com/docs/mongoose-os/howtos/trix.md

// Конфигурационные параметры
#define DT_LIMIT          (mgos_sys_config_get_app_dt_level() / 10.0f)
#define VENT_PIN          mgos_sys_config_get_app_gpio_vent_pin()
#define WD_PIN           mgos_sys_config_get_app_WDDonePin()
#define WIRE_PIN         mgos_sys_config_get_app_gpio_wire_pin()
#define SENSOR_READ_MS   mgos_sys_config_get_app_sensor_read_interval_ms()
#define WD_TIMER_MS      mgos_sys_config_get_app_wdTimer()



// Глобальные переменные состояния
static OneWire *ow = NULL;
static DallasTemperature *sensors = NULL;
static char sensor1_addr[8] = {0};
static char sensor2_addr[8] = {0};

static float t1 = 0.0, t2 = 0.0, t1_last = 0.0, tmin = 0.0;
static bool is_on = false, previous_change = false, temp_down = false;
static bool rpc_timer = false, ignore_up = false;
static char reason[32] = {0};
static mgos_timer_id timer_id = MGOS_INVALID_TIMER_ID;
static int num_devices = 0;

// Прототипы функций
static void read_sensors_cb(void *arg);
static void wd_tick_cb(void *arg);
static void shadow_send(void);
static void vent_set(bool state);

// Инициализация датчиков температуры
static void init_temp_sensors() {
  ow = mgos_arduino_onewire_create(WIRE_PIN);
  if (ow == NULL) {
    LOG(LL_ERROR, ("Failed to initialize OneWire"));
    return;
  }

  sensors = mgos_arduino_dt_create(ow);
  if (sensors == NULL) {
    LOG(LL_ERROR, ("Failed to initialize Dallas Temperature"));
    return;
  }

  mgos_arduino_dt_begin(sensors);
  num_devices = mgos_arduino_dt_get_device_count(sensors);
  LOG(LL_INFO, ("Found %d devices", num_devices));

  if (num_devices >= 1) {
    if (!mgos_arduino_dt_get_address(sensors, sensor1_addr, 0)) {
      LOG(LL_ERROR, ("Failed to get address for sensor 1"));
    }
  }

  if (num_devices >= 2) {
    if (!mgos_arduino_dt_get_address(sensors, sensor2_addr, 1)) {
      LOG(LL_ERROR, ("Failed to get address for sensor 2"));
    }
  }

  // mgos_onewire_addr_to_str(sensor1_addr, addr_str);
  LOG(LL_INFO, ("Sensor 1: %s", sensor1_addr));
  // mgos_onewire_addr_to_str(sensor2_addr, addr_str);
  LOG(LL_INFO, ("Sensor 2: %s", sensor2_addr));
}

static void rpc_timer_finish_cb(void *arg) {
  rpc_timer = false;
  timer_id = MGOS_INVALID_TIMER_ID;
  (void) arg;
}

// RPC methods
// curl -H 'Content-Type: application-json' -d '{"key": "BATH_55831A", "minutes": 2}' https://mdash.net/api/v2/devices/device10/rpc/on_warmer?access_token=kZWZ91uczT99CC22khdGpTdw
// curl 192.168.1.11/rpc/off_vent -d '{"minutes":1, "key": "HOOD_48DF97", "on_off": "on"}'
// {"command":"R","ip":"${esp.ip}","method":"/rpc/off_vent","minutes":5, "key": "${esp.id}", "on_off": ${!esp.is_on} }
// Проверить, что текущий режим не равен переданному
// Если уже запущен таймер  - остановить его
// Если минуты >0  - создать Включить нужный режим, установить флаг, гарантирующий, что автоматом не будет переключаться  вентилятор, отправить сообщение в mdash, запустить таймер 
// После срабатывания таймера снять флаг, установить в корректное  состояние is_on и previos_change

// В основном цикле проверять флаг, если стоит - не переключать вытяжку 

// Обработчик RPC для управления вентилятором
static void rpc_off_vent_cb(struct mg_rpc_request_info *ri, void *cb_arg,
                           struct mg_rpc_frame_info *fi, struct mg_str args) {
  int minutes = 0;
  bool on_off = false;
  const char *key = NULL;

  json_scanf(args.p, args.len, "{minutes: %d, key: %Q, on_off: %B}",
             &minutes, &key, &on_off);
  LOG(LL_INFO, ("minutes:%d, key: %s d2:%zu, strcmp: %d onof:  %d %d", minutes, key, strlen(key),  strcmp(key, mgos_sys_config_get_device_id()), on_off, is_on));
  if (minutes <= 0 || key == NULL || strcmp(key, mgos_sys_config_get_device_id()) != 0 || on_off == is_on) {
    mg_rpc_send_errorf(ri, -1, mgos_sys_config_get_device_id());
    return;
  }

  if (timer_id != MGOS_INVALID_TIMER_ID) {
    mgos_clear_timer(timer_id);
    timer_id = MGOS_INVALID_TIMER_ID;
  }

  is_on = on_off;
  rpc_timer = true;
  strcpy(reason, "rpc");
  vent_set(is_on);

  timer_id = mgos_set_timer(minutes * 60000, 0, rpc_timer_finish_cb, NULL);

  shadow_send();
  mg_rpc_send_responsef(ri, NULL);
  (void) cb_arg;
  (void) fi;
}

static void system_restart_cb(void *arg) {
  mgos_system_restart();
  (void) arg;
}


// Обработчик RPC для настройки WiFi
static void rpc_setwifi_cb(struct mg_rpc_request_info *ri, void *cb_arg,
    struct mg_rpc_frame_info *fi, struct mg_str args) {
    struct json_token config_tok;
    const char *config = NULL;
    const char *key = NULL;

    // Парсим аргументы
    if (json_scanf(args.p, args.len, "{config: %T, key: %Q}", 
        &config_tok, &key) != 2) {
        mg_rpc_send_errorf(ri, -1, "Invalid parameters");
        return;
    }

    config = config_tok.ptr;

    LOG(LL_INFO, ("WiFi config request: %.*s", (int)config_tok.len, config));
    LOG(LL_INFO, ("WiFi key request: %s",  key));
    LOG(LL_INFO, (" mgos_sys_config_get_device_id: %s", mgos_sys_config_get_device_id()));
    LOG(LL_INFO, ("d1:%zu, d2:%zu, strcmp: %d", strlen(key), strlen(mgos_sys_config_get_device_id()),  strcmp(key, mgos_sys_config_get_device_id())));

    // Проверяем ключ устройства
    if (strcmp(key, mgos_sys_config_get_device_id()) != 0) {
        mg_rpc_send_errorf(ri, -1, mgos_sys_config_get_device_id());
        return;
    }

    // Сохраняем конфигурацию
    if (!mgos_config_apply(config, true)) {
        LOG(LL_ERROR, ("Failed to save WiFi config"));
        mg_rpc_send_errorf(ri, -1, "Config save error");
        return;
    }

    LOG(LL_INFO, ("WiFi config saved, rebooting..."));
    mg_rpc_send_responsef(ri, "System rebooting");

    // Перезагрузка через 500 мс
    mgos_set_timer(500, 0, system_restart_cb, NULL);

    (void) cb_arg;
    (void) fi;
}

// Управление вентилятором
static void vent_set(bool state) {
  mgos_gpio_write(VENT_PIN, state ? 0 : 1);
}

// Отправка данных в Shadow
static void shadow_send(void) {
    char msg[256];
    snprintf(msg, sizeof(msg),
            "{\"t1\": %.2f, \"t2\": %.2f, \"is_on\": %s, \"tmin\": %.2f, \"reason\": \"%s\", \"ver\":\"v3\"}",
            t1, t2, is_on ? "true" : "false", tmin, reason);
    mgos_shadow_updatef(0, "%s", msg);
    LOG(LL_INFO, ("%s", msg));
    // Формируем данные для mDash DB.Save
    char dash_msg[256];
    snprintf(dash_msg, sizeof(dash_msg),
             "[%.2f, %.2f, %d, %.2f, \"%s\", \"v3\"]",
             t1, t2, is_on, tmin, reason);
  
    // Отправляем событие в mDash
    if (mgos_dash_is_connected()) {
      mgos_dash_notify("DB.Save", dash_msg);
      LOG(LL_INFO, ("mDash DB.Save: %s", dash_msg));
    } else {
      LOG(LL_WARN, ("mDash not connected, skipping DB.Save"));
    }
}

// Watchdog таймер
static void wd_tick_cb(void *arg) {
  mgos_gpio_write(WD_PIN, 1);
  mgos_usleep(2);
  mgos_gpio_write(WD_PIN, 0);
  (void) arg;
}

// Обработчик события обновления времени
static void time_changed_cb(int ev, void *ev_data, void *userdata) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    LOG(LL_INFO, ("System time updated: %s", time_str));
    
    (void) ev;
    (void) ev_data;
    (void) userdata;
  }

// Основная логика обработки температур
static void read_sensors_cb(void *arg) {
    if (num_devices == 0) {
        t1 += 0.1f;
        t2 += 0.1f;
    }else{
        mgos_arduino_dt_request_temperatures(sensors);
        t1 = mgos_arduino_dt_get_tempc(sensors, sensor1_addr)/100.0;
        t2 = mgos_arduino_dt_get_tempc(sensors, sensor2_addr)/100.0;
    }  
      // включить если разница температур между датчиками  выше  заданной  на DT_LIMIT+0.5 (dt>=DT_LIMIT+0.5 && !is_on)
      // включить если разница  между предыдущей и текущей температурой  >0.1  (t1-t_last>0.1 && !is_on)
      // выключить если  температура упала ниже запомненной  ранее температуры  включения (t1<tmin && is_on)
      // выключить если температура уменьшилась на 0,1 и уменьшение  два раза подряд (t_last-t1>0.1 && temp_down && is_on)
      // выключить, если два раза подряд  температура понижается
      // проблема  - корпус или сам вентилятор нагревается при работе плиты  работающий вентилятор охлаждает датчик. t1 уменьшается? вентилятор выключается. Когда вентилятор отключается, корпус начинает греть датчик, температура  повышается, включается вентилятор. Гипотеза - если отслеживать и предыдущий  шаг, то времени хватит  на охлаждение - надо не повторять  вкл/вкл каждый цикл 


  if (t1 > 0.0 && t2 > 0.0) {
    reason[0] = '\0';
    
    if (((t1 - t1_last > 0.5f && t1_last > 0.0 && !is_on && !ignore_up) ||
        (t1 < tmin && is_on) ||
        (t1_last - t1 > 0.1f && t1_last > 0.0 && temp_down && is_on) ||
        (temp_down && t1_last > t1 && is_on)) && !previous_change && !rpc_timer) {
// работает отлично температура  нижнего датчика выросла на 0,1 по сравлению с предыдущим измерением
      if (t1 - t1_last > 0.5f && t1_last > 0 && !is_on) strcat(reason, " +0.5");
//Выключить , если температора  нижнего датчика  меньше температуры верхнего датчика в момент включения вытяжки
      if (t1 < tmin && is_on) strcat(reason, " tmin");
// работает отлично температура  нижнего датчика упала на 0,1 по сравлению с предыдущим измерением и предыдущее  значение  температуры было выше
      if (t1_last - t1 > 0.1f && t1_last > 0 && temp_down && is_on) strcat(reason, " -0.1");
 // температура падает дважды, менее строгая проверка предыдущего  варианта
      if (temp_down && t1_last > t1 && is_on) {
        strcat(reason, " -х2");
        //проигнорировать включение в следующем цикле при повышении  T  для компенсации охлаждения корпуса
        ignore_up = true;
      }

      is_on = !is_on;
      vent_set(is_on);
    } else {
      previous_change = false;
      ignore_up = false;
    }

    temp_down = (t1 < t1_last);
    t1_last = t1;

    if (tmin < 0.0 && is_on) tmin = t2;
    if (!is_on && tmin > 0.0) tmin = -1.0;
  }

  shadow_send();
  (void) arg;
}

// Инициализация приложения
enum mgos_app_init_result mgos_app_init(void) {
  // Инициализация GPIO
  mgos_gpio_set_mode(VENT_PIN, MGOS_GPIO_MODE_OUTPUT);
  vent_set(false);
  
  mgos_gpio_set_mode(WD_PIN, MGOS_GPIO_MODE_OUTPUT);
  mgos_gpio_write(WD_PIN, 0);

  // Инициализация датчиков температуры
  init_temp_sensors();

  // Настройка таймеров
  mgos_set_timer(SENSOR_READ_MS, MGOS_TIMER_REPEAT, read_sensors_cb, NULL);
  mgos_set_timer(WD_TIMER_MS, MGOS_TIMER_REPEAT, wd_tick_cb, NULL);

  // Регистрация RPC метода
  mg_rpc_add_handler(mgos_rpc_get_global(), "off_vent", "", rpc_off_vent_cb, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "setwifi", "", rpc_setwifi_cb, NULL);

    // Регистрируем обработчик события обновления времени
    mgos_event_add_handler(MGOS_EVENT_TIME_CHANGED, time_changed_cb, NULL);

  return MGOS_APP_INIT_SUCCESS;
}