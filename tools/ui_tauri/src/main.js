let invokeFn = null;

function appendFatal(message) {
  let box = document.getElementById("fatal-log");
  if (!box) {
    box = document.createElement("pre");
    box.id = "fatal-log";
    box.style.whiteSpace = "pre-wrap";
    box.style.padding = "12px";
    box.style.margin = "12px";
    box.style.border = "1px solid #f5c2c2";
    box.style.background = "#fff1f1";
    box.style.color = "#8a1c1c";
    box.style.borderRadius = "8px";
    document.body.prepend(box);
  }
  box.textContent += `${message}\n`;
}

async function initTauriApi() {
  const tauriGlobal = window.__TAURI__;
  const candidates = [
    tauriGlobal?.core?.invoke,
    tauriGlobal?.tauri?.invoke,
    tauriGlobal?.invoke,
    window.__TAURI_INTERNALS__?.invoke
  ];
  invokeFn = candidates.find((fn) => typeof fn === "function") ?? null;

  if (invokeFn) return;

  try {
    const core = await import("../node_modules/@tauri-apps/api/core.js");
    invokeFn = core.invoke;
  } catch (error) {
    appendFatal(`[ERROR] Tauri API 加载失败: ${error}`);
  }
}

const invoke = async (cmd, args = {}) => {
  if (!invokeFn) {
    throw new Error("Tauri invoke 不可用");
  }
  return invokeFn(cmd, args);
};

const ZONE_TYPES = ["NONE", "MOUSE", "KEYBOARD"];
const MOUSE_BUTTONS = [["无", 0], ["左键", 1], ["右键", 2], ["中键", 4]];
const ZONES = [
  ["leftTop", "左上角"], ["rightTop", "右上角"], ["rightBottom", "右下角"], ["leftBottom", "左下角"],
  ["threeLeft", "三指左滑"], ["threeRight", "三指右滑"], ["threeUp", "三指上滑"], ["threeDown", "三指下滑"],
  ["threeTap", "三指单击"], ["threeDoubleTap", "三指双击"],
  ["fourLeft", "四指左滑"], ["fourRight", "四指右滑"], ["fourUp", "四指上滑"], ["fourDown", "四指下滑"],
  ["fourTap", "四指单击"], ["fourDoubleTap", "四指双击"]
];

const HID_KEY_MAP = new Map([
  ["KeyA",4],["KeyB",5],["KeyC",6],["KeyD",7],["KeyE",8],["KeyF",9],["KeyG",10],["KeyH",11],["KeyI",12],["KeyJ",13],["KeyK",14],["KeyL",15],["KeyM",16],["KeyN",17],["KeyO",18],["KeyP",19],["KeyQ",20],["KeyR",21],["KeyS",22],["KeyT",23],["KeyU",24],["KeyV",25],["KeyW",26],["KeyX",27],["KeyY",28],["KeyZ",29],
  ["Digit1",30],["Digit2",31],["Digit3",32],["Digit4",33],["Digit5",34],["Digit6",35],["Digit7",36],["Digit8",37],["Digit9",38],["Digit0",39],
  ["Enter",40],["Escape",41],["Backspace",42],["Tab",43],["Space",44],["Minus",45],["Equal",46],["BracketLeft",47],["BracketRight",48],["Backslash",49],["Semicolon",51],["Quote",52],["Backquote",53],["Comma",54],["Period",55],["Slash",56],
  ["CapsLock",57],["F1",58],["F2",59],["F3",60],["F4",61],["F5",62],["F6",63],["F7",64],["F8",65],["F9",66],["F10",67],["F11",68],["F12",69],
  ["PrintScreen",70],["ScrollLock",71],["Pause",72],["Insert",73],["Home",74],["PageUp",75],["Delete",76],["End",77],["PageDown",78],["ArrowRight",79],["ArrowLeft",80],["ArrowDown",81],["ArrowUp",82]
]);

const scalar = {};
const zones = {};

function el(tag, cls, text) {
  const n = document.createElement(tag);
  if (cls) n.className = cls;
  if (text) n.textContent = text;
  return n;
}

function addField(container, label, input) {
  const row = el("label", "field");
  const span = el("span", "field-label", label);
  row.append(span, input);
  container.appendChild(row);
}

function numberInput(min, max, step, value) {
  const i = document.createElement("input");
  i.type = "number";
  i.min = String(min);
  i.max = String(max);
  i.step = String(step);
  if (value != null) i.value = String(value);
  return i;
}

function checkbox(text) {
  const wrap = el("label", "checkbox-wrap");
  const i = document.createElement("input");
  i.type = "checkbox";
  wrap.append(i, el("span", "", text));
  return [wrap, i];
}

function zoneWidget() {
  const type = document.createElement("select");
  ZONE_TYPES.forEach((t) => type.append(new Option(t, t)));
  const buttons = document.createElement("select");
  MOUSE_BUTTONS.forEach(([n, v]) => buttons.append(new Option(n, String(v))));
  const keyView = document.createElement("input");
  keyView.readOnly = true;
  keyView.placeholder = "未绑定";
  const rec = el("button", "btn", "录制");
  rec.type = "button";
  const w = { type, buttons, keyView, rec, modifier: 0, key: 0 };
  const sync = () => {
    const isMouse = type.value === "MOUSE";
    const isKey = type.value === "KEYBOARD";
    buttons.disabled = !isMouse;
    rec.disabled = !isKey;
    keyView.disabled = !isKey;
  };
  type.addEventListener("change", sync);
  sync();
  rec.addEventListener("click", () => captureHotkey(w));
  return w;
}

function keyDisplay(mod, key) {
  if (!key) return "未绑定";
  const names = [];
  if (mod & 0x01) names.push("Ctrl");
  if (mod & 0x02) names.push("Shift");
  if (mod & 0x04) names.push("Alt");
  if (mod & 0x08) names.push("Win");
  names.push(`KEY_${key}`);
  return names.join("+");
}

function captureHotkey(w) {
  const overlay = el("div", "overlay");
  const box = el("div", "dialog");
  box.textContent = "请按下要绑定的快捷键（Esc 取消）";
  overlay.appendChild(box);
  document.body.appendChild(overlay);

  const onKey = (e) => {
    e.preventDefault();
    if (e.key === "Escape") {
      cleanup();
      return;
    }
    if (["Shift", "Control", "Alt", "Meta"].includes(e.key)) return;
    const hid = HID_KEY_MAP.get(e.code) || HID_KEY_MAP.get(e.key);
    if (!hid) {
      box.textContent = "该按键不支持，请重试（Esc 取消）";
      return;
    }
    let mod = 0;
    if (e.ctrlKey) mod |= 0x01;
    if (e.shiftKey) mod |= 0x02;
    if (e.altKey) mod |= 0x04;
    if (e.metaKey) mod |= 0x08;
    w.type.value = "KEYBOARD";
    w.modifier = mod;
    w.key = hid;
    w.keyView.value = keyDisplay(mod, hid);
    w.type.dispatchEvent(new Event("change"));
    cleanup();
  };

  function cleanup() {
    window.removeEventListener("keydown", onKey, true);
    overlay.remove();
  }

  window.addEventListener("keydown", onKey, true);
}

function log(msg) {
  const box = document.getElementById("log");
  box.value += `${msg}\n`;
  box.scrollTop = box.scrollHeight;
}

async function refreshPorts() {
  const select = scalar.port;
  select.innerHTML = "";
  const ports = await invoke("list_ports");
  ports.forEach((p) => select.append(new Option(p, p)));
  scalar.status.textContent = ports.length ? "未连接" : "无串口";
}

function setZoneFromData(prefix, data) {
  const z = zones[prefix];
  if (!z) return;
  const t = data[`${prefix}Type`];
  if (t) z.type.value = t.toUpperCase();
  if (data[`${prefix}Buttons`] != null) z.buttons.value = String(data[`${prefix}Buttons`]);
  if (data[`${prefix}Modifier`] != null) z.modifier = Number(data[`${prefix}Modifier`]);
  if (data[`${prefix}Key`] != null) z.key = Number(data[`${prefix}Key`]);
  z.keyView.value = keyDisplay(z.modifier, z.key);
  z.type.dispatchEvent(new Event("change"));
}

async function readValues() {
  const data = await invoke("get_all");
  const v = data.values;
  const bindNum = (k, i) => { if (v[k] != null) i.value = v[k]; };
  const bindBool = (k, i) => { if (v[k] != null) i.checked = v[k] === "1"; };

  bindNum("scrollSensitivity", scalar.scrollSensitivity);
  bindNum("scrollLowSpeedBoostEnd", scalar.scrollLowSpeedBoostEnd);
  bindNum("scrollMinActiveSpeed", scalar.scrollMinActiveSpeed);
  bindNum("scrollAccelStartSpeed", scalar.scrollAccelStartSpeed);
  bindNum("scrollAccelFactor", scalar.scrollAccelFactor);
  bindNum("scrollMaxAccel", scalar.scrollMaxAccel);
  bindNum("sensitivity", scalar.sensitivity);
  bindNum("smoothFactor", scalar.smoothFactor);
  bindNum("accelFactor", scalar.accelFactor);
  bindNum("maxAccel", scalar.maxAccel);
  bindNum("maxDelta", scalar.maxDelta);
  bindNum("moveDeadband", scalar.moveDeadband);
  bindNum("rate", scalar.rate);
  bindBool("useBleWhenUsb", scalar.useBleWhenUsb);
  bindBool("bleIdleSleepEnabled", scalar.bleIdleSleepEnabled);
  bindNum("bleIdleLightMs", scalar.bleIdleLightMs);
  bindNum("bleIdleMediumMs", scalar.bleIdleMediumMs);
  bindNum("bleIdleSleepMs", scalar.bleIdleSleepMs);
  bindNum("lightIdleRate", scalar.lightIdleRate);
  bindNum("topZonePercent", scalar.topZonePercent);
  bindNum("sideZonePercent", scalar.sideZonePercent);
  bindBool("enableNavZones", scalar.enableNavZones);
  bindNum("threeSwipeThresholdX", scalar.threeSwipeThresholdX);
  bindNum("threeSwipeThresholdY", scalar.threeSwipeThresholdY);
  bindNum("threeSwipeTimeout", scalar.threeSwipeTimeout);
  bindNum("threeSwipeCooldown", scalar.threeSwipeCooldown);
  bindNum("fourSwipeThresholdX", scalar.fourSwipeThresholdX);
  bindNum("fourSwipeThresholdY", scalar.fourSwipeThresholdY);
  bindNum("fourSwipeTimeout", scalar.fourSwipeTimeout);
  bindNum("fourSwipeCooldown", scalar.fourSwipeCooldown);

  ZONES.forEach(([k]) => setZoneFromData(k, v));
  log("已读取当前值");
}

function zoneCmd(prefix, w) {
  return [
    `SET ${prefix}Type ${w.type.value}`,
    `SET ${prefix}Buttons ${w.buttons.value}`,
    `SET ${prefix}Modifier ${w.modifier}`,
    `SET ${prefix}Key ${w.key}`
  ];
}

async function applyAll() {
  const cmds = [
    `SET scrollSensitivity ${Number(scalar.scrollSensitivity.value).toFixed(8)}`,
    `SET sensitivity ${Number(scalar.sensitivity.value).toFixed(3)}`,
    `SET smoothFactor ${Number(scalar.smoothFactor.value).toFixed(3)}`,
    `SET accelFactor ${Number(scalar.accelFactor.value).toFixed(3)}`,
    `SET maxAccel ${Number(scalar.maxAccel.value).toFixed(2)}`,
    `SET maxDelta ${scalar.maxDelta.value}`,
    `SET moveDeadband ${scalar.moveDeadband.value}`,
    `SET topZonePercent ${scalar.topZonePercent.value}`,
    `SET sideZonePercent ${scalar.sideZonePercent.value}`,
    `SET enableNavZones ${scalar.enableNavZones.checked ? 1 : 0}`,
    `SET rate ${scalar.rate.value}`,
    `SET scrollLowSpeedBoostEnd ${Number(scalar.scrollLowSpeedBoostEnd.value).toFixed(3)}`,
    `SET scrollMinActiveSpeed ${Number(scalar.scrollMinActiveSpeed.value).toFixed(3)}`,
    `SET scrollAccelStartSpeed ${Number(scalar.scrollAccelStartSpeed.value).toFixed(3)}`,
    `SET scrollAccelFactor ${Number(scalar.scrollAccelFactor.value).toFixed(3)}`,
    `SET scrollMaxAccel ${Number(scalar.scrollMaxAccel.value).toFixed(2)}`,
    `SET useBleWhenUsb ${scalar.useBleWhenUsb.checked ? 1 : 0}`,
    `SET bleIdleSleepEnabled ${scalar.bleIdleSleepEnabled.checked ? 1 : 0}`,
    `SET bleIdleLightMs ${scalar.bleIdleLightMs.value}`,
    `SET bleIdleMediumMs ${scalar.bleIdleMediumMs.value}`,
    `SET bleIdleSleepMs ${scalar.bleIdleSleepMs.value}`,
    `SET lightIdleRate ${scalar.lightIdleRate.value}`,
    `SET threeSwipeThresholdX ${scalar.threeSwipeThresholdX.value}`,
    `SET threeSwipeThresholdY ${scalar.threeSwipeThresholdY.value}`,
    `SET threeSwipeTimeout ${scalar.threeSwipeTimeout.value}`,
    `SET threeSwipeCooldown ${scalar.threeSwipeCooldown.value}`,
    `SET fourSwipeThresholdX ${scalar.fourSwipeThresholdX.value}`,
    `SET fourSwipeThresholdY ${scalar.fourSwipeThresholdY.value}`,
    `SET fourSwipeTimeout ${scalar.fourSwipeTimeout.value}`,
    `SET fourSwipeCooldown ${scalar.fourSwipeCooldown.value}`
  ];
  ZONES.forEach(([k]) => cmds.push(...zoneCmd(k, zones[k])));

  for (const cmd of cmds) {
    const lines = await invoke("send_command", { command: cmd });
    lines.forEach(log);
  }
}

async function simpleCmd(cmd, reload = false) {
  const lines = await invoke("send_command", { command: cmd });
  lines.forEach(log);
  if (reload) await readValues();
}

function section(title) {
  const s = el("section", "card");
  s.appendChild(el("h2", "", title));
  return s;
}

function initUI() {
  const app = document.getElementById("app");
  app.innerHTML = "";

  const top = section("连接");
  const conn = el("div", "row");
  scalar.port = document.createElement("select");
  const refresh = el("button", "btn", "刷新");
  const connect = el("button", "btn", "连接");
  scalar.status = el("span", "status", "未连接");
  conn.append(el("span", "pill", "USB串口"), scalar.port, refresh, connect, scalar.status);
  top.appendChild(conn);
  app.appendChild(top);

  const hint = el("p", "hint", "提示：当前 nRF 固件已移除 BLE 调参，仅支持 USB 串口调参。");
  app.appendChild(hint);

  const form = el("div", "grid");
  app.appendChild(form);

  const scroll = section("滚动");
  scalar.scrollSensitivity = numberInput(0, 1, 0.000001, 0.00002);
  scalar.scrollLowSpeedBoostEnd = numberInput(0.001, 2, 0.01, 0.3);
  scalar.scrollMinActiveSpeed = numberInput(0, 2, 0.01, 0.1);
  scalar.scrollAccelStartSpeed = numberInput(0, 2, 0.01, 0.3);
  scalar.scrollAccelFactor = numberInput(0, 5, 0.05, 0.5);
  scalar.scrollMaxAccel = numberInput(1, 10, 0.1, 3.0);
  addField(scroll, "滚动灵敏度", scalar.scrollSensitivity);
  addField(scroll, "低速扶正终点", scalar.scrollLowSpeedBoostEnd);
  addField(scroll, "最小滚动速度", scalar.scrollMinActiveSpeed);
  addField(scroll, "滚动加速起点", scalar.scrollAccelStartSpeed);
  addField(scroll, "滚动加速系数", scalar.scrollAccelFactor);
  addField(scroll, "滚动最大加速", scalar.scrollMaxAccel);
  form.appendChild(scroll);

  const single = section("单指");
  scalar.sensitivity = numberInput(0.01, 5, 0.01, 0.8);
  scalar.smoothFactor = numberInput(0, 1, 0.01, 0.2);
  scalar.accelFactor = numberInput(0, 1, 0.01, 0.2);
  scalar.maxAccel = numberInput(0, 10, 0.1, 2.0);
  scalar.maxDelta = numberInput(1, 200, 1, 70);
  scalar.moveDeadband = numberInput(0, 20, 1, 2);
  addField(single, "灵敏度", scalar.sensitivity);
  addField(single, "平滑系数", scalar.smoothFactor);
  addField(single, "加速系数", scalar.accelFactor);
  addField(single, "最大加速", scalar.maxAccel);
  addField(single, "单次最大位移", scalar.maxDelta);
  addField(single, "移动死区", scalar.moveDeadband);
  form.appendChild(single);

  const power = section("上报/省电");
  scalar.rate = numberInput(10, 200, 1, 120);
  addField(power, "频率(Hz)", scalar.rate);
  [scalar.useBleWhenUsbWrap, scalar.useBleWhenUsb] = checkbox("USB连接时仍允许 BLE");
  [scalar.bleIdleSleepEnabledWrap, scalar.bleIdleSleepEnabled] = checkbox("启用空闲省电");
  scalar.bleIdleLightMs = numberInput(1000, 3600000, 1000, 600000);
  scalar.bleIdleMediumMs = numberInput(1000, 3600000, 1000, 1800000);
  scalar.bleIdleSleepMs = numberInput(1000, 3600000, 1000, 3600000);
  scalar.lightIdleRate = numberInput(5, 120, 1, 20);
  power.append(scalar.useBleWhenUsbWrap, scalar.bleIdleSleepEnabledWrap);
  addField(power, "第一阶段触发时长(ms)", scalar.bleIdleLightMs);
  addField(power, "第二阶段触发时长(ms)", scalar.bleIdleMediumMs);
  addField(power, "第三阶段触发时长(ms)", scalar.bleIdleSleepMs);
  addField(power, "轻度空闲频率(Hz)", scalar.lightIdleRate);
  form.appendChild(power);

  const zone = section("区域");
  scalar.topZonePercent = numberInput(5, 50, 1, 20);
  scalar.sideZonePercent = numberInput(5, 50, 1, 35);
  [scalar.enableNavZonesWrap, scalar.enableNavZones] = checkbox("启用区域功能");
  addField(zone, "上边高度百分比", scalar.topZonePercent);
  addField(zone, "左右宽度百分比", scalar.sideZonePercent);
  zone.appendChild(scalar.enableNavZonesWrap);
  form.appendChild(zone);

  const bind = section("手势/区域绑定");
  ZONES.forEach(([k, name]) => {
    const w = zoneWidget();
    zones[k] = w;
    addField(bind, `${name} 类型`, w.type);
    addField(bind, `${name} 鼠标按钮`, w.buttons);
    addField(bind, `${name} 绑定结果`, w.keyView);
    addField(bind, `${name} 录制快捷键`, w.rec);
  });
  scalar.threeSwipeThresholdX = numberInput(50, 800, 1, 200);
  scalar.threeSwipeThresholdY = numberInput(50, 800, 1, 200);
  scalar.threeSwipeTimeout = numberInput(50, 1000, 1, 350);
  scalar.threeSwipeCooldown = numberInput(0, 2000, 1, 400);
  scalar.fourSwipeThresholdX = numberInput(50, 800, 1, 200);
  scalar.fourSwipeThresholdY = numberInput(50, 800, 1, 200);
  scalar.fourSwipeTimeout = numberInput(50, 1000, 1, 350);
  scalar.fourSwipeCooldown = numberInput(0, 2000, 1, 400);
  addField(bind, "三指水平阈值", scalar.threeSwipeThresholdX);
  addField(bind, "三指垂直阈值", scalar.threeSwipeThresholdY);
  addField(bind, "三指滑动超时(ms)", scalar.threeSwipeTimeout);
  addField(bind, "三指冷却时间(ms)", scalar.threeSwipeCooldown);
  addField(bind, "四指水平阈值", scalar.fourSwipeThresholdX);
  addField(bind, "四指垂直阈值", scalar.fourSwipeThresholdY);
  addField(bind, "四指滑动超时(ms)", scalar.fourSwipeTimeout);
  addField(bind, "四指冷却时间(ms)", scalar.fourSwipeCooldown);
  form.appendChild(bind);

  const ops = section("操作");
  const opsRow = el("div", "row");
  const apply = el("button", "btn primary", "应用");
  const read = el("button", "btn", "读取当前值");
  const save = el("button", "btn", "保存");
  const load = el("button", "btn", "加载");
  const reset = el("button", "btn danger", "恢复默认");
  opsRow.append(apply, read, save, load, reset);
  ops.appendChild(opsRow);
  app.appendChild(ops);

  const logs = section("日志");
  const logBox = document.createElement("textarea");
  logBox.id = "log";
  logBox.readOnly = true;
  logs.appendChild(logBox);
  app.appendChild(logs);

  refresh.addEventListener("click", async () => {
    try { await refreshPorts(); } catch (e) { log(`刷新失败: ${e}`); }
  });

  connect.addEventListener("click", async () => {
    try {
      const connected = await invoke("is_connected");
      if (connected) {
        await invoke("disconnect_serial");
        scalar.status.textContent = "未连接";
        connect.textContent = "连接";
        return;
      }
      if (!scalar.port.value) {
        log("未选择设备");
        return;
      }
      await invoke("connect_serial", { port: scalar.port.value, baud: 115200 });
      scalar.status.textContent = "已连接";
      connect.textContent = "断开";
      await readValues();
    } catch (e) {
      log(`连接失败: ${e}`);
    }
  });

  apply.addEventListener("click", async () => { try { await applyAll(); } catch (e) { log(`应用失败: ${e}`); } });
  read.addEventListener("click", async () => { try { await readValues(); } catch (e) { log(`读取失败: ${e}`); } });
  save.addEventListener("click", async () => { try { await simpleCmd("SAVE"); } catch (e) { log(`保存失败: ${e}`); } });
  load.addEventListener("click", async () => { try { await simpleCmd("LOAD", true); } catch (e) { log(`加载失败: ${e}`); } });
  reset.addEventListener("click", async () => { try { await simpleCmd("RESET", true); } catch (e) { log(`重置失败: ${e}`); } });
}

async function bootstrap() {
  try {
    await initTauriApi();
    initUI();
    await refreshPorts();
  } catch (error) {
    appendFatal(`[ERROR] 启动失败: ${error}`);
  }
}

bootstrap().catch((error) => appendFatal(`[ERROR] 未捕获异常: ${error}`));
