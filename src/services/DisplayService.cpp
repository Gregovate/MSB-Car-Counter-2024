#include "traffic/Firmware.h"

// Extracted from the production baseline; behavior fixes are separate changes.
void updateDisplay() {
    if (!countsReady()) {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        display.setCursor(0,line1);
        display.println("COUNT RECOVERY");
        display.println("Pending - check HA");
        display.display();
        return;
    }
    DateTime now = rtc.now();
    //float tempF = ((rtc.getTemperature() * 9 / 5) + 32); // Get temperature in Fahrenheit
    int currentHr24 = now.hour();
    int currentHr12 = currentHr24 > 12 ? currentHr24 - 12 : (currentHr24 == 0 ? 12 : currentHr24);
    const char* ampm = currentHr24 < 12 ? "AM" : "PM";

    // Clear display and set formatting
    display.clearDisplay();
    display.setTextColor(WHITE);
    //display.setFont(&FreeSans12pt7b);

    // Line 1: Date and Day of the Week
    display.setTextSize(1);
    display.setCursor(0, line1);
    display.printf("%s %s %02d, %04d", days[now.dayOfTheWeek()], months[now.month() - 1], now.day(), now.year());

    // Line 2: Time and Temperature
    display.setCursor(0, line2);
    display.printf("%02d:%02d:%02d %s  %d F", currentHr12, now.minute(), now.second(), ampm, (int)tempF);

    // Line 3: Days Running and Show Total
    display.setCursor(0, line3);
    display.printf("Day %d  Total: %d", daysRunning, totalShowCars);

    // Line 4: Total Daily Cars
    display.setTextSize(2);
    display.setCursor(0, 40);
    display.printf("Cars: %d", totalDailyCars);

    // Write to the display
    display.display();
}
