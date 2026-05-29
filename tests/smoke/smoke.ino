// Smoke test sketch — verifies the PCMFlowG711 library compiles against
// the chosen profile and that the test harness wiring works.
// Once the codec implementation lands, this stays as a build-only check.

void setup()
{
    Serial.begin(115200);
    delay(5000);
    Serial.println("SMOKE ready");
}

void loop()
{
    delay(1);
}
