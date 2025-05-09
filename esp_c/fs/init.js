load('api_sys.js');
load('api_config.js');
load('api_net.js');
load('api_events.js');

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
  print('== Net event:', ev, evs);
}, null);



