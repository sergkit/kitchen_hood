load('api_timer.js');
load('api_sys.js');
load('api_config.js');
load('api_gpio.js');
load('api_net.js');
load('api_rpc.js');
load('api_events.js');
load('api_arduino_dallas_temp.js');
load('api_shadow.js');
load('api_arduino_onewire.js');
load('api_dash.js');


let t1=0;
let t2=0;
let DT_LIMIT=Cfg.get("app.dt_level")/10;
let is_on=false;
let previos_change= false;
let tmin=0;
let t1_last=0;
let rpc_timer = false;
let temp_down = false;
let reason='';
let ignore_up = false;
let timer_id = null;

let vent_pin = Cfg.get("app.gpio_vent_pin");
GPIO.set_mode(vent_pin, GPIO.MODE_OUTPUT);
GPIO.write(vent_pin, 1);

// let light_pin = Cfg.get("app.gpio_light_pin");
// GPIO.set_mode(light_pin, GPIO.MODE_OUTPUT);
// GPIO.write(light_pin, 1);

let ow = OneWire.create(Cfg.get("app.gpio_wire_pin"));
let myDT = DallasTemperature.create(ow);
myDT.begin()
let n = myDT.getDeviceCount();
print ('getDeviceCount: ', n)

let sens = [];

for (let i = 0; i < n; i++) {
  sens[i] = '01234567';
  if (myDT.getAddress(sens[i], i) === 1) {
    print('Sensor#', i, 'Address:', myDT.toHexStr(sens[i]));
  }
}


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



let timerState=Timer.set(Cfg.get("app.sensor_read_interval_ms"), Timer.REPEAT, function () {
  
  if(n>0){
    myDT.requestTemperatures();
    t1= myDT.getTempC(sens[0]);
    t2= myDT.getTempC(sens[1]);
  }else{
    t1=t1+1;
    t2= t2+1; 
  }

  let  dt=0;
  if (t1>0 && t2>0){
    if (t1>t2) {
      dt=t1-t2;
    } else{
      dt=0;
    }
    // включить если разница температур между датчиками  выше  заданной  на DT_LIMIT+0.5 (dt>=DT_LIMIT+0.5 && !is_on)
    // включить если разница  между предыдущей и текущей температурой  >0.1  (t1-t_last>0.1 && !is_on)
    // выключить если  температура упала ниже запомненной  ранее температуры  включения (t1<tmin && is_on)
    // выключить если температура уменьшилась на 0,1 и уменьшение  два раза подряд (t_last-t1>0.1 && temp_down && is_on)
    // выключить, если два раза подряд  температура понижается
    // проблема  - корпус или сам вентилятор нагревается при работе плиты  работающий вентилятор охлаждает датчик. t1 уменьшается? вентилятор выключается. Когда вентилятор отключается, корпус начинает греть датчик, температура  повышается, включается вентилятор. Гипотеза - если отслеживать и предыдущий  шаг, то времени хватит  на охлаждение - надо не повторять  вкл/вкл каждый цикл 
    reason="";
    
    if ((
      // (dt>=DT_LIMIT+0.5 && !is_on) || 
          (t1-t1_last>0.5 &&  t1_last>0 && !is_on && !ignore_up)   ||
          (t1<tmin && is_on) || 
          (t1_last-t1>0.1 &&  t1_last>0 && temp_down && is_on)  ||
          (temp_down && t1_last>t1 && is_on)
     ) && !previos_change  && !rpc_timer)   {

      
      // if (dt>=DT_LIMIT+0.5 && !is_on) { // не работает, включается не вовремя
      //   reason= reason+ " dt";
      // }
      if (t1-t1_last>0.5 && t1_last>0 && !is_on) { // работает отлично температура  нижнего датчика выросла на 0,1 по сравлению с предыдущим измерением
        reason=reason+ " +0.5";
      }
      if (t1<tmin && is_on) { //Выключить , если температора  нижнего датчика  меньше температуры верхнего датчика в момент включения вытяжки
        reason=reason+ " tmin";
      }
      if (t1_last-t1>0.1 &&  t1_last>0 && temp_down && is_on) { // работает отлично температура  нижнего датчика упала на 0,1 по сравлению с предыдущим измерением и предыдущее  значение  температуры было выше
        reason=reason+ " -0.1";
      }
      if (temp_down && t1_last>t1 && is_on) { // температура падает дважды, менее строгая проверка предыдущего  варианта
        reason=reason+ " -х2";
        ignore_up=true; //проигнорировать включение в следующем цикле при повышении  T  для компенсации охлаждения корпуса
      }
      // previos_change=true;
      is_on=!is_on;
      // переключение  выводов
      let w=1;
      if (is_on){
        w = 0;
      }
      // GPIO.write(light_pin, w);
      GPIO.write(vent_pin, w);

    
    }else{
      previos_change = false;
      ignore_up = false;
    }
    if (t1<t1_last) {
      temp_down = true;
    } else {
      temp_down = false;
    }
    t1_last=t1;
    if (tmin === 0 && is_on){
      tmin=t2;
    }
    if (!is_on && (tmin > 0)){
      tmin = 0;
    }
  }
  shadowSend();


}, null);

//wd timer
let wd_pin = Cfg.get("app.WDDonePin");
GPIO.set_mode(wd_pin, GPIO.MODE_OUTPUT);
GPIO.write(wd_pin, 0);

Timer.set(Cfg.get("app.wdTimer"), Timer.REPEAT, function () {
  GPIO.write(wd_pin, 1);
  Sys.usleep(2);
  GPIO.write(wd_pin, 0);

}, null);


let shadowSend = function ( ) {
    let obj = {t1:t1, t2:t2, is_on:is_on,  obj: { u: Sys.uptime(), r: Sys.free_ram() }};
    Shadow.update(0, obj);
    print( JSON.stringify(obj));
    if (Dash.isConnected()){
      Dash.notify("DB.Save",[t1, t2, is_on, tmin, reason, "v2"]);
    }
};


// RPC methods
// curl -H 'Content-Type: application-json' -d '{"key": "BATH_55831A", "minutes": 2}' https://mdash.net/api/v2/devices/device10/rpc/on_warmer?access_token=kZWZ91uczT99CC22khdGpTdw
// curl 192.168.1.11/rpc/off_vent -d '{"minutes":1, "key": "HOOD_48DF97", "on_off": "on"}'
// {"command":"R","ip":"${esp.ip}","method":"/rpc/off_vent","minutes":5, "key": "${esp.id}", "on_off": ${!esp.is_on} }
// Проверить, что текущий режим не равен переданному
// Если уже запущен таймер  - остановить его
// Если минуты >0  - создать Включить нужный режим, установить флаг, гарантирующий, что автоматом не будет переключаться  вентилятор, отправить сообщение в mdash, запустить таймер 
// После срабатывания таймера снять флаг, установить в корректное  состояние is_on и previos_change

// В основном цикле проверять флаг, если стоит - не переключать вытяжку 

RPC.addHandler('off_vent', function (args) {

  if (typeof (args) === 'object'
    && typeof (args.minutes) === 'number'
    && typeof (args.key) === "string"
    && typeof (args.on_off) === "boolean"
    && args.key === Cfg.get('device.id')) {
      // if (args.minutes > 0 && is_on !== args.on_off){
      //   if(timer_id > 0){
      //     Timer.del(timer_id);
      //   }
      //   is_on = args.on_off;
      //   rpc_timer = true;
      //   // переключение  выводов
      //   let w=1;
      //   if (is_on){
      //     w = 0;
      //   }
      //   reason="rpc";
      //   GPIO.write(vent_pin, w);
      //   is_on !== args.on_off
      //   timer_id = Timer.set(args.minutes * 60000, 0, function () {
      //     timer_id = null;
      //     rpc_timer = false;
  
      //   }, null);
      //   shadowSend();
      // }

    return { error: 0, message: "off_vent Ок" };
  } else {
    return { error: -1, message: Cfg.get('device.id') };
  }    

});



// RPC methods
RPC.addHandler('setwifi', function (args) {
  if (typeof (args) === 'object'
    && typeof (args.config) === 'object'
    && typeof (args.key) === "string"
    && args.key === Cfg.get('device.id')) {
    print('Request:', JSON.stringify(args.config));
    print('Rsave:', Cfg.set(args.config, true));
    Sys.reboot(500);
    return "System rebooting";
  } else {
    return { error: -1, message: Cfg.get('device.id') };
  }
});



// Monitor network connectivity.
Event.addGroupHandler(Net.EVENT_GRP, function (ev, evdata, arg) {
  let evs = '???';
  if (ev === Net.STATUS_DISCONNECTED && Cfg.get('wifi.ap.enable') === false) {
    evs = 'DISCONNECTED';
    Cfg.set({ wifi: { ap: { enable: true } } });
    Sys.reboot(500);
  } else if (ev === Net.STATUS_CONNECTING) {
    evs = 'CONNECTING';
  } else if (ev === Net.STATUS_CONNECTED) {
    evs = 'CONNECTED';
    print("wifi.ap.enable");
    print(Cfg.get('wifi.ap.enable'));
  } else if (ev === Net.STATUS_GOT_IP && Cfg.get('wifi.ap.enable') === true) {
    evs = 'GOT_IP';
    Cfg.set({ wifi: { ap: { enable: false } } });
    Sys.reboot(500);
    print(Cfg.get('wifi.ap.enable'));
  }
  if (ev === Net.STATUS_GOT_IP) {
  }

  print('== Net event:', ev, evs);
}, null);



// generic time change, includes SNTP (SYS+3)
Event.addHandler(Event.OTA_TIME_CHANGED, function (ev, evdata, ud) {
  print('Time set');
}, null);