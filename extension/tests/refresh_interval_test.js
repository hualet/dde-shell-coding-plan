let passed = 0;
let failed = 0;

function assert(condition, message) {
  if (!condition) {
    console.error("FAIL: " + message);
    failed++;
  } else {
    passed++;
  }
}

function assertEqual(actual, expected, message) {
  if (actual !== expected) {
    console.error("FAIL: " + message + " — expected " + JSON.stringify(expected) + " got " + JSON.stringify(actual));
    failed++;
  } else {
    passed++;
  }
}

const storageState = {};

globalThis.chrome = {
  storage: {
    local: {
      async get(key) {
        if (typeof key === "string") {
          return { [key]: storageState[key] };
        }
        const result = {};
        for (const k of Object.keys(key)) {
          if (storageState[k] !== undefined) result[k] = storageState[k];
        }
        return result;
      },
      async set(obj) {
        Object.assign(storageState, obj);
      },
    },
  },
};

function resetStorage() {
  for (const k of Object.keys(storageState)) {
    delete storageState[k];
  }
}

const {
  clampRefreshInterval,
  getRefreshInterval,
  setRefreshInterval,
  REFRESH_INTERVAL_DEFAULT,
  REFRESH_INTERVAL_MIN,
  REFRESH_INTERVAL_MAX,
  REFRESH_INTERVAL_STORAGE_KEY,
} = await import("../shared/storage.js");

// --- clampRefreshInterval tests ---

assertEqual(clampRefreshInterval(30), 30, "clamp 30 => 30 (default, in range)");
assertEqual(clampRefreshInterval(5), 5, "clamp 5 => 5 (min boundary)");
assertEqual(clampRefreshInterval(120), 120, "clamp 120 => 120 (max boundary)");
assertEqual(clampRefreshInterval(1), REFRESH_INTERVAL_MIN, "clamp 1 => min (below range)");
assertEqual(clampRefreshInterval(200), REFRESH_INTERVAL_MAX, "clamp 200 => max (above range)");
assertEqual(clampRefreshInterval(0), REFRESH_INTERVAL_MIN, "clamp 0 => min");
assertEqual(clampRefreshInterval(-5), REFRESH_INTERVAL_MIN, "clamp -5 => min");
assertEqual(clampRefreshInterval(10.4), 10, "clamp 10.4 => 10 (rounds down)");
assertEqual(clampRefreshInterval(10.5), 11, "clamp 10.5 => 11 (rounds up)");
assertEqual(clampRefreshInterval(10.7), 11, "clamp 10.7 => 11 (rounds up)");
assertEqual(clampRefreshInterval(NaN), REFRESH_INTERVAL_DEFAULT, "clamp NaN => default 30");
assertEqual(clampRefreshInterval(Infinity), REFRESH_INTERVAL_MAX, "clamp Infinity => max 120");
assertEqual(clampRefreshInterval(-Infinity), REFRESH_INTERVAL_MIN, "clamp -Infinity => min 5");
assertEqual(clampRefreshInterval("50"), 50, "clamp '50' => 50 (string number)");
assertEqual(clampRefreshInterval("abc"), REFRESH_INTERVAL_DEFAULT, "clamp 'abc' => default 30");
assertEqual(clampRefreshInterval(null), REFRESH_INTERVAL_DEFAULT, "clamp null => default 30");
assertEqual(clampRefreshInterval(undefined), REFRESH_INTERVAL_DEFAULT, "clamp undefined => default 30");

// --- getRefreshInterval tests ---

resetStorage();
{
  const val = await getRefreshInterval();
  assertEqual(val, REFRESH_INTERVAL_DEFAULT, "getRefreshInterval with empty storage => default 30");
}

{
  storageState[REFRESH_INTERVAL_STORAGE_KEY] = 10;
  const val = await getRefreshInterval();
  assertEqual(val, 10, "getRefreshInterval with stored 10 => 10");
}

{
  storageState[REFRESH_INTERVAL_STORAGE_KEY] = 200;
  const val = await getRefreshInterval();
  assertEqual(val, REFRESH_INTERVAL_MAX, "getRefreshInterval with stored 200 => clamped to max 120");
}

{
  storageState[REFRESH_INTERVAL_STORAGE_KEY] = 1;
  const val = await getRefreshInterval();
  assertEqual(val, REFRESH_INTERVAL_MIN, "getRefreshInterval with stored 1 => clamped to min 5");
}

{
  storageState[REFRESH_INTERVAL_STORAGE_KEY] = "abc";
  const val = await getRefreshInterval();
  assertEqual(val, REFRESH_INTERVAL_DEFAULT, "getRefreshInterval with stored 'abc' => default 30");
}

{
  storageState[REFRESH_INTERVAL_STORAGE_KEY] = 50;
  const val = await getRefreshInterval();
  assertEqual(val, 50, "getRefreshInterval with stored 50 => 50");
}

// --- setRefreshInterval tests ---

resetStorage();
{
  const saved = await setRefreshInterval(15);
  assertEqual(saved, 15, "setRefreshInterval(15) returns 15");
  assertEqual(storageState[REFRESH_INTERVAL_STORAGE_KEY], 15, "setRefreshInterval(15) stores 15");
}

{
  const saved = await setRefreshInterval(3);
  assertEqual(saved, REFRESH_INTERVAL_MIN, "setRefreshInterval(3) returns clamped min 5");
  assertEqual(storageState[REFRESH_INTERVAL_STORAGE_KEY], REFRESH_INTERVAL_MIN, "setRefreshInterval(3) stores clamped min 5");
}

{
  const saved = await setRefreshInterval(500);
  assertEqual(saved, REFRESH_INTERVAL_MAX, "setRefreshInterval(500) returns clamped max 120");
  assertEqual(storageState[REFRESH_INTERVAL_STORAGE_KEY], REFRESH_INTERVAL_MAX, "setRefreshInterval(500) stores clamped max 120");
}

{
  const saved = await setRefreshInterval("25");
  assertEqual(saved, 25, "setRefreshInterval('25') returns 25");
  assertEqual(storageState[REFRESH_INTERVAL_STORAGE_KEY], 25, "setRefreshInterval('25') stores 25");
}

{
  const saved = await setRefreshInterval(REFRESH_INTERVAL_DEFAULT);
  assertEqual(saved, REFRESH_INTERVAL_DEFAULT, "setRefreshInterval(default) returns default 30");
  assertEqual(storageState[REFRESH_INTERVAL_STORAGE_KEY], REFRESH_INTERVAL_DEFAULT, "setRefreshInterval(default) stores default 30");
}

// --- Same-value skip simulation ---

resetStorage();
{
  await setRefreshInterval(30);
  const oldVal = clampRefreshInterval(storageState[REFRESH_INTERVAL_STORAGE_KEY]);
  await setRefreshInterval(30);
  const newVal = clampRefreshInterval(storageState[REFRESH_INTERVAL_STORAGE_KEY]);
  assertEqual(oldVal, newVal, "same-value set produces equal normalized values");
  assert(oldVal === newVal, "same-value comparison: old === new, would skip reschedule");
}

{
  await setRefreshInterval(30);
  const oldVal = clampRefreshInterval(storageState[REFRESH_INTERVAL_STORAGE_KEY]);
  await setRefreshInterval(29.7);
  const newVal = clampRefreshInterval(storageState[REFRESH_INTERVAL_STORAGE_KEY]);
  assertEqual(oldVal, newVal, "29.7 rounds to 30, same as current 30, would skip reschedule");
}

{
  await setRefreshInterval(30);
  const oldVal = clampRefreshInterval(storageState[REFRESH_INTERVAL_STORAGE_KEY]);
  await setRefreshInterval(29);
  const newVal = clampRefreshInterval(storageState[REFRESH_INTERVAL_STORAGE_KEY]);
  assert(oldVal !== newVal, "29 normalizes to 29, different from 30, would reschedule");
}

// --- Constants consistency ---

assertEqual(REFRESH_INTERVAL_DEFAULT, 30, "REFRESH_INTERVAL_DEFAULT is 30");
assertEqual(REFRESH_INTERVAL_MIN, 5, "REFRESH_INTERVAL_MIN is 5");
assertEqual(REFRESH_INTERVAL_MAX, 120, "REFRESH_INTERVAL_MAX is 120");
assert(REFRESH_INTERVAL_MIN < REFRESH_INTERVAL_DEFAULT, "MIN < DEFAULT");
assert(REFRESH_INTERVAL_DEFAULT < REFRESH_INTERVAL_MAX, "DEFAULT < MAX");

console.log("\n=== Refresh Interval Test Results ===");
console.log("Passed: " + passed);
console.log("Failed: " + failed);
if (failed > 0) {
  console.error("\nSOME TESTS FAILED");
  process.exit(1);
} else {
  console.log("\nALL TESTS PASSED");
}
