let passed = 0;
let failed = 0;

export function assert(condition, message) {
  if (!condition) {
    console.error("FAIL: " + message);
    failed++;
  } else {
    passed++;
  }
}

export function assertEqual(actual, expected, message) {
  const eps = 1e-9;
  if (typeof expected === "number" && typeof actual === "number") {
    if (Math.abs(actual - expected) > eps) {
      console.error("FAIL: " + message + " — expected " + JSON.stringify(expected) + " got " + JSON.stringify(actual));
      failed++;
      return;
    }
  } else if (actual !== expected) {
    console.error("FAIL: " + message + " — expected " + JSON.stringify(expected) + " got " + JSON.stringify(actual));
    failed++;
    return;
  }
  passed++;
}

export function printSummary(name) {
  console.log(`\n${name}: ${passed} passed, ${failed} failed`);
  if (failed > 0) {
    process.exit(1);
  }
}
