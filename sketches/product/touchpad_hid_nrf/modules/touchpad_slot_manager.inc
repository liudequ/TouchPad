static const uint8_t kSlotCount = 3;

struct SlotBinding {
  bool bonded;
  char addr[18];
  char name[32];
};

static bool slotManagerEnabled = false;
static uint8_t slotActive = 1;
static uint8_t slotConnected = 0;
static uint8_t slotPairing = 0;
static uint8_t slotReplaceArmed = 0;
static SlotBinding slotBindings[kSlotCount];

bool slotIndexValid(uint8_t slot) {
  return slot >= 1 && slot <= kSlotCount;
}

void slotClearBinding(uint8_t slot) {
  if (!slotIndexValid(slot)) return;
  SlotBinding* b = &slotBindings[slot - 1];
  b->bonded = false;
  b->addr[0] = '\0';
  b->name[0] = '\0';
}

void slotResetAll() {
  slotActive = 1;
  slotConnected = 0;
  slotPairing = 0;
  slotReplaceArmed = 0;
  for (uint8_t i = 1; i <= kSlotCount; i++) {
    slotClearBinding(i);
  }
}

void slotPeerAddrToString(uint16_t conn_handle, char* out, size_t outSize) {
  if (!out || outSize < 18) return;
  out[0] = '\0';
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  if (!connection) return;
  ble_gap_addr_t addr = connection->getPeerAddr();
  snprintf(out, outSize, "%02X:%02X:%02X:%02X:%02X:%02X",
           addr.addr[5], addr.addr[4], addr.addr[3],
           addr.addr[2], addr.addr[1], addr.addr[0]);
}

void slotPeerNameToString(uint16_t conn_handle, char* out, size_t outSize) {
  if (!out || outSize < 2) return;
  out[0] = '\0';
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  if (!connection) return;
  uint16_t n = connection->getPeerName(out, (uint16_t)(outSize - 1));
  if (n == 0) {
    snprintf(out, outSize, "unknown");
  } else {
    out[outSize - 1] = '\0';
  }
}

int8_t slotFindByAddr(const char* addr) {
  if (!addr || !addr[0]) return -1;
  for (uint8_t i = 0; i < kSlotCount; i++) {
    if (!slotBindings[i].bonded) continue;
    if (strcmp(slotBindings[i].addr, addr) == 0) return (int8_t)(i + 1);
  }
  return -1;
}

void slotBindPeer(uint8_t slot, const char* addr, const char* name) {
  if (!slotIndexValid(slot)) return;
  SlotBinding* b = &slotBindings[slot - 1];
  b->bonded = true;
  snprintf(b->addr, sizeof(b->addr), "%s", addr ? addr : "");
  snprintf(b->name, sizeof(b->name), "%s", name ? name : "unknown");
}

void slotPrintStatus() {
  cfgOut->print("slotManagerEnabled=");
  cfgOut->println(slotManagerEnabled ? "1" : "0");
  cfgOut->print("slotActive=");
  cfgOut->println(slotActive);
  cfgOut->print("slotConnected=");
  cfgOut->println(slotConnected);
  cfgOut->print("slotPairing=");
  cfgOut->println(slotPairing);
  for (uint8_t i = 0; i < kSlotCount; i++) {
    cfgOut->print("slot");
    cfgOut->print(i + 1);
    cfgOut->print("Bonded=");
    cfgOut->println(slotBindings[i].bonded ? "1" : "0");
    cfgOut->print("slot");
    cfgOut->print(i + 1);
    cfgOut->print("Addr=");
    cfgOut->println(slotBindings[i].addr);
    cfgOut->print("slot");
    cfgOut->print(i + 1);
    cfgOut->print("Name=");
    cfgOut->println(slotBindings[i].name);
  }
}

void slotPrintCompactStatus() {
  cfgOut->print("slot=");
  cfgOut->print(slotActive);
  cfgOut->print(",connected=");
  cfgOut->print(slotConnected);
  cfgOut->print(",pairing=");
  cfgOut->println(slotPairing);
  for (uint8_t i = 0; i < kSlotCount; i++) {
    cfgOut->print("s");
    cfgOut->print(i + 1);
    cfgOut->print(":");
    cfgOut->print(slotBindings[i].bonded ? "1" : "0");
    cfgOut->print(",");
    cfgOut->print(slotBindings[i].addr[0] ? slotBindings[i].addr : "-");
    cfgOut->print(",");
    cfgOut->println(slotBindings[i].name[0] ? slotBindings[i].name : "-");
  }
}

bool slotSelect(uint8_t slot) {
  if (!slotIndexValid(slot)) {
    cfgOut->println("ERR: slot");
    return false;
  }
  slotActive = slot;
  slotPairing = 0;
  slotReplaceArmed = 0;
  if (Bluefruit.connected()) {
    Bluefruit.disconnect(0);
    delay(50);
  }
  if (useBleTransport() && !advSuppressedByIdle) {
    startAdv();
  }
  cfgOut->print("OK: SLOT ");
  cfgOut->println(slot);
  return true;
}

bool slotStartPair(uint8_t slot, bool forceReplace) {
  if (!slotManagerEnabled) {
    cfgOut->println("ERR: slot manager disabled");
    return false;
  }
  if (!slotIndexValid(slot)) {
    cfgOut->println("ERR: slot");
    return false;
  }
  if (slotBindings[slot - 1].bonded && !forceReplace) {
    slotReplaceArmed = slot;
    cfgOut->print("CONFIRM: PAIR SLOT ");
    cfgOut->print(slot);
    cfgOut->println(" FORCE");
    return false;
  }
  slotReplaceArmed = 0;
  slotActive = slot;
  slotPairing = slot;
  slotClearBinding(slot);
  if (Bluefruit.connected()) {
    Bluefruit.disconnect(0);
    delay(50);
  }
  if (useBleTransport() && !advSuppressedByIdle) {
    startAdv();
  }
  cfgOut->print("OK: PAIR SLOT ");
  cfgOut->println(slot);
  return true;
}

bool slotUnpair(uint8_t slot) {
  if (!slotManagerEnabled) {
    cfgOut->println("ERR: slot manager disabled");
    return false;
  }
  if (!slotIndexValid(slot)) {
    cfgOut->println("ERR: slot");
    return false;
  }
  slotReplaceArmed = 0;
  slotClearBinding(slot);
  if (slotConnected == slot && Bluefruit.connected()) {
    Bluefruit.disconnect(0);
    delay(50);
  }
  if (slotActive == slot) slotPairing = 0;
  cfgOut->print("OK: UNPAIR SLOT ");
  cfgOut->println(slot);
  return true;
}

void slotOnConnect(uint16_t conn_handle) {
  if (!slotManagerEnabled) return;
  slotReplaceArmed = 0;

  char addr[18];
  char name[32];
  slotPeerAddrToString(conn_handle, addr, sizeof(addr));
  slotPeerNameToString(conn_handle, name, sizeof(name));

  if (slotPairing && slotIndexValid(slotPairing)) {
    slotBindPeer(slotPairing, addr, name);
    slotConnected = slotPairing;
    slotActive = slotPairing;
    slotPairing = 0;
    return;
  }

  int8_t matched = slotFindByAddr(addr);
  if (matched > 0) {
    slotConnected = (uint8_t)matched;
    slotActive = (uint8_t)matched;
    return;
  }

  if (slotIndexValid(slotActive) && !slotBindings[slotActive - 1].bonded) {
    slotBindPeer(slotActive, addr, name);
    slotConnected = slotActive;
    return;
  }

  slotConnected = 0;
}

void slotOnDisconnect() {
  if (!slotManagerEnabled) return;
  slotConnected = 0;
}

void slotSaveConfig(File& f) {
  f.print("slotManagerEnabled=");
  f.println(slotManagerEnabled ? "1" : "0");
  f.print("slotActive=");
  f.println(slotActive);
  for (uint8_t i = 0; i < kSlotCount; i++) {
    f.print("slot");
    f.print(i + 1);
    f.print("Bonded=");
    f.println(slotBindings[i].bonded ? "1" : "0");
    f.print("slot");
    f.print(i + 1);
    f.print("Addr=");
    f.println(slotBindings[i].addr);
    f.print("slot");
    f.print(i + 1);
    f.print("Name=");
    f.println(slotBindings[i].name);
  }
}

bool slotLoadConfigEntry(const String& key, const String& value) {
  if (key.equalsIgnoreCase("slotManagerEnabled")) {
    slotManagerEnabled = (value == "1" || value.equalsIgnoreCase("true"));
    return true;
  }
  if (key.equalsIgnoreCase("slotActive")) {
    int v = value.toInt();
    if (slotIndexValid((uint8_t)v)) slotActive = (uint8_t)v;
    return true;
  }

  for (uint8_t i = 0; i < kSlotCount; i++) {
    String base = String("slot") + String(i + 1);
    if (key.equalsIgnoreCase(base + "Bonded")) {
      slotBindings[i].bonded = (value == "1" || value.equalsIgnoreCase("true"));
      return true;
    }
    if (key.equalsIgnoreCase(base + "Addr")) {
      snprintf(slotBindings[i].addr, sizeof(slotBindings[i].addr), "%s", value.c_str());
      return true;
    }
    if (key.equalsIgnoreCase(base + "Name")) {
      snprintf(slotBindings[i].name, sizeof(slotBindings[i].name), "%s", value.c_str());
      return true;
    }
  }
  return false;
}
