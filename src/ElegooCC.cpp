#include "ElegooCC.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "FilamentFlowTracker.h"
#include "Logger.h"
#include "SettingsManager.h"

#define ACK_TIMEOUT_MS 5000
constexpr float        MOVEMENT_MM_PER_TOGGLE                = 2.8f;
constexpr float        DEFAULT_FILAMENT_DEFICIT_THRESHOLD_MM = 8.4f;
constexpr unsigned int EXPECTED_FILAMENT_SAMPLE_MS           = 250;
constexpr unsigned int EXPECTED_FILAMENT_STALE_MS            = 1000;
constexpr unsigned int SDCP_LOSS_TIMEOUT_MS                  = 10000;
constexpr unsigned int PAUSE_REARM_DELAY_MS                  = 3000;
static const char*     TOTAL_EXTRUSION_HEX_KEY       = "54 6F 74 61 6C 45 78 74 72 75 73 69 6F 6E 00";
static const char*     CURRENT_EXTRUSION_HEX_KEY =
    "43 75 72 72 65 6E 74 45 78 74 72 75 73 69 6F 6E 00";
static const uint16_t  SDCP_DISCOVERY_PORT = 30000;

// External function to get current time (from main.cpp)
extern unsigned long getTime();

ElegooCC &ElegooCC::getInstance()
{
    static ElegooCC instance;
    return instance;
}

ElegooCC::ElegooCC()
{
    lastMovementValue = -1;
    lastChangeTime    = 0;

    mainboardID       = "";
    printStatus       = SDCP_PRINT_STATUS_IDLE;
    machineStatusMask = 0;  // No statuses active initially
    currentLayer      = 0;
    totalLayer        = 0;
    progress          = 0;
    currentTicks      = 0;
    totalTicks        = 0;
    PrintSpeedPct     = 0;
    filamentStopped   = false;
    filamentRunout    = false;
    lastPing          = 0;
    expectedFilamentMM         = 0;
    actualFilamentMM           = 0;
    lastExpectedDeltaMM        = 0;
    expectedTelemetryAvailable = false;
    lastSuccessfulTelemetryMs  = 0;
    lastTelemetryReceiveMs     = 0;
    lastStatusReceiveMs          = 0;
    telemetryAvailableLastStatus = false;
    currentDeficitMm             = 0.0f;
    deficitThresholdMm           = 0.0f;
    deficitRatio                 = 0.0f;
    movementPulseCount           = 0;
    lastFlowLogMs                = 0;
    lastSummaryLogMs             = 0;
    flowTracker.reset();

    waitingForAck       = false;
    pendingAckCommand   = -1;
    pendingAckRequestId = "";
    ackWaitStartTime    = 0;
    lastPauseRequestMs  = 0;

    // TODO: send a UDP broadcast, M99999 on Port 30000, maybe using AsyncUDP.h and listen for the
    // result. this will give us the printer IP address.

    // event handler - use lambda to capture 'this' pointer
    webSocket.onEvent([this](WStype_t type, uint8_t *payload, size_t length)
                      { this->webSocketEvent(type, payload, length); });
}

void ElegooCC::setup()
{
    bool shouldConect = !settingsManager.isAPMode();
    if (shouldConect)
    {
        connect();
    }
}

void ElegooCC::webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
        case WStype_DISCONNECTED:
            logger.log("Disconnected from Carbon Centauri");
            // Reset acknowledgment state on disconnect
            waitingForAck       = false;
            pendingAckCommand   = -1;
            pendingAckRequestId = "";
            ackWaitStartTime    = 0;
            break;
        case WStype_CONNECTED:
            logger.log("Connected to Carbon Centauri");
            sendCommand(SDCP_COMMAND_STATUS);

            break;
        case WStype_TEXT:
        {
            StaticJsonDocument<2048> doc;
            DeserializationError     error = deserializeJson(doc, payload);

            if (error)
            {
                logger.logf("JSON parsing failed: %s", error.c_str());
                return;
            }

            // Check if this is a command acknowledgment response
            if (doc.containsKey("Id") && doc.containsKey("Data"))
            {
                handleCommandResponse(doc);
            }
            // Check if this is a status response
            else if (doc.containsKey("Status"))
            {
                handleStatus(doc);
            }
        }
        break;
        case WStype_BIN:
            logger.log("Received unspported binary data");
            break;
        case WStype_ERROR:
            logger.logf("WebSocket error: %s", payload);
            break;
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            logger.log("Received unspported fragment data");
            break;
    }
}

void ElegooCC::handleCommandResponse(JsonDocument &doc)
{
    String     id   = doc["Id"];
    JsonObject data = doc["Data"];

    if (data.containsKey("Cmd") && data.containsKey("RequestID"))
    {
        int    cmd         = data["Cmd"];
        int    ack         = data["Data"]["Ack"];
        String requestId   = data["RequestID"];
        String mainboardId = data["MainboardID"];

        logger.logf("Command %d acknowledged (Ack: %d) for request %s", cmd, ack,
                    requestId.c_str());

        // Check if this is the acknowledgment we're waiting for
        if (waitingForAck && cmd == pendingAckCommand && requestId == pendingAckRequestId)
        {
            logger.logf("Received expected acknowledgment for command %d", cmd);
            waitingForAck       = false;
            pendingAckCommand   = -1;
            pendingAckRequestId = "";
            ackWaitStartTime    = 0;
        }

        // Store mainboard ID if we don't have it yet
        if (mainboardID.isEmpty() && !mainboardId.isEmpty())
        {
            mainboardID = mainboardId;
            logger.logf("Stored MainboardID: %s", mainboardID.c_str());
        }
    }
}

void ElegooCC::handleStatus(JsonDocument &doc)
{
    JsonObject status      = doc["Status"];
    String     mainboardId = doc["MainboardID"];
    unsigned long statusTimestamp = millis();
    lastStatusReceiveMs          = statusTimestamp;

    // Parse current status (which contains machine status array)
    if (status.containsKey("CurrentStatus"))
    {
        JsonArray currentStatus = status["CurrentStatus"];

        // Convert JsonArray to int array for machine statuses
        int statuses[5];  // Max 5 statuses
        int count = min((int) currentStatus.size(), 5);
        for (int i = 0; i < count; i++)
        {
            statuses[i] = currentStatus[i].as<int>();
        }

        // Set all machine statuses at once
        setMachineStatuses(statuses, count);
    }

    // Parse CurrentCoords to extract Z coordinate
    if (status.containsKey("CurrenCoord"))
    {
        String coordsStr   = status["CurrenCoord"].as<String>();
        int    firstComma  = coordsStr.indexOf(',');
        int    secondComma = coordsStr.indexOf(',', firstComma + 1);
        if (firstComma != -1 && secondComma != -1)
        {
            String zStr = coordsStr.substring(secondComma + 1);
            currentZ    = zStr.toFloat();
        }
    }

    // Parse print info
    if (status.containsKey("PrintInfo"))
    {
        JsonObject          printInfo = status["PrintInfo"];
        sdcp_print_status_t newStatus = printInfo["Status"].as<sdcp_print_status_t>();
        if (newStatus != printStatus)
        {
            if (newStatus == SDCP_PRINT_STATUS_PRINTING)
            {
                logger.log("Print status changed to printing");
                startedAt = millis();
                resetFilamentTracking();
            }
        }
        printStatus   = newStatus;
        currentLayer  = printInfo["CurrentLayer"];
        totalLayer    = printInfo["TotalLayer"];
        progress      = printInfo["Progress"];
        currentTicks  = printInfo["CurrentTicks"];
        totalTicks    = printInfo["TotalTicks"];
        PrintSpeedPct                = printInfo["PrintSpeedPct"];
        telemetryAvailableLastStatus = processFilamentTelemetry(printInfo, statusTimestamp);

        if (settingsManager.getVerboseLogging())
        {
            logger.logf(
                "Flow debug: SDCP status print=%d layer=%d/%d progress=%d expected=%.3fmm "
                "delta=%.3fmm telemetry=%d",
                (int) printStatus, currentLayer, totalLayer, progress, expectedFilamentMM,
                lastExpectedDeltaMM, telemetryAvailableLastStatus ? 1 : 0);
        }
    }

    // Store mainboard ID if we don't have it yet (I'm unsure if we actually need this)
    if (mainboardID.isEmpty() && !mainboardId.isEmpty())
    {
        mainboardID = mainboardId;
        logger.logf("Stored MainboardID: %s", mainboardID.c_str());
    }
}

void ElegooCC::resetFilamentTracking()
{
    lastMovementValue          = -1;
    lastChangeTime             = millis();
    actualFilamentMM           = 0;
    expectedFilamentMM         = 0;
    lastExpectedDeltaMM        = 0;
    expectedTelemetryAvailable = false;
    lastSuccessfulTelemetryMs  = 0;
    filamentStopped            = false;
    lastTelemetryReceiveMs     = 0;
    movementPulseCount         = 0;
    currentDeficitMm           = 0.0f;
    deficitThresholdMm         = 0.0f;
    deficitRatio               = 0.0f;
    lastFlowLogMs              = 0;
    flowTracker.reset();
}

void ElegooCC::updateExpectedFilament(unsigned long currentTime)
{
    if (expectedTelemetryAvailable &&
        (currentTime - lastTelemetryReceiveMs) > EXPECTED_FILAMENT_STALE_MS)
    {
        expectedTelemetryAvailable = false;
        if (!settingsManager.getKeepExpectedForever())
        {
            flowTracker.reset();
        }
    }
}

bool ElegooCC::tryReadExtrusionValue(JsonObject &printInfo, const char *key, const char *hexKey,
                                     float &output)
{
    if (printInfo.containsKey(key) && !printInfo[key].isNull())
    {
        output = printInfo[key].as<float>();
        return true;
    }

    if (hexKey != nullptr && printInfo.containsKey(hexKey) && !printInfo[hexKey].isNull())
    {
        output = printInfo[hexKey].as<float>();
        return true;
    }

    return false;
}

bool ElegooCC::processFilamentTelemetry(JsonObject &printInfo, unsigned long currentTime)
{
    float totalValue = 0;
    float deltaValue = 0;
    bool  hasTotal   = tryReadExtrusionValue(printInfo, "TotalExtrusion", TOTAL_EXTRUSION_HEX_KEY,
                                            totalValue);
    bool  hasDelta   = tryReadExtrusionValue(printInfo, "CurrentExtrusion",
                                            CURRENT_EXTRUSION_HEX_KEY, deltaValue);

    if (!hasTotal && !hasDelta)
    {
        expectedTelemetryAvailable   = false;
        telemetryAvailableLastStatus = false;
        return false;
    }

    telemetryAvailableLastStatus = true;
    expectedTelemetryAvailable   = true;
    lastSuccessfulTelemetryMs  = currentTime;
    lastTelemetryReceiveMs     = currentTime;

    if (hasTotal)
    {
        expectedFilamentMM = totalValue < 0 ? 0 : totalValue;
    }

    if (hasDelta)
    {
        lastExpectedDeltaMM = deltaValue;
        if (deltaValue > 0)
        {
            unsigned long holdMs = settingsManager.getExpectedFlowWindowMs();
            if (holdMs == 0)
            {
                holdMs = EXPECTED_FILAMENT_STALE_MS;
            }
            unsigned long pruneWindow;
            if (settingsManager.getKeepExpectedForever())
            {
                // Effectively disable time-based pruning for expected filament.
                pruneWindow = 0xFFFFFFFFUL;
            }
            else
            {
                pruneWindow = holdMs * 2;
            }
            flowTracker.addExpected(deltaValue, currentTime, pruneWindow);
        }
    }

    return true;
}

void ElegooCC::pausePrint()
{
    if (settingsManager.getDevMode())
    {
        lastPauseRequestMs = millis();
        logger.log("Dev mode is enabled: pausePrint suppressed (would send pause command)");
        return;
    }
    lastPauseRequestMs = millis();
    sendCommand(SDCP_COMMAND_PAUSE_PRINT, true);
}

void ElegooCC::continuePrint()
{
    sendCommand(SDCP_COMMAND_CONTINUE_PRINT, true);
}

void ElegooCC::sendCommand(int command, bool waitForAck)
{
    if (!webSocket.isConnected())
    {
        logger.logf("Can't send command, websocket not connected: %d", command);
        return;
    }

    // If this command requires an ack and we're already waiting for one, skip it
    if (waitForAck && waitingForAck)
    {
        logger.logf("Skipping command %d - already waiting for ack from command %d", command,
                    pendingAckCommand);
        return;
    }

    uuid.generate();
    String uuidStr = String(uuid.toCharArray());
    uuidStr.replace("-", "");  // RequestID doesn't want dashes

    // Get current timestamp
    unsigned long timestamp = getTime();

    StaticJsonDocument<512> doc;
    doc["Id"] = uuidStr;
    JsonObject data = doc.createNestedObject("Data");
    data["Cmd"]     = command;
    data["RequestID"]   = uuidStr;
    data["MainboardID"] = mainboardID;
    data["TimeStamp"]   = timestamp;
    data["From"]        = 2;  // 0: OctoEverywhere, 1: web client, 2: this device

    // Explicit empty Data object (matches existing payload structure)
    data.createNestedObject("Data");

    // Include current SDCP print and machine status, mirroring the status payload fields.
    data["PrintStatus"] = static_cast<int>(printStatus);
    JsonArray currentStatus = data.createNestedArray("CurrentStatus");
    for (int s = 0; s <= 4; ++s)
    {
        if (hasMachineStatus(static_cast<sdcp_machine_status_t>(s)))
        {
            currentStatus.add(s);
        }
    }

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    // If this command requires an ack, set the tracking state
    if (waitForAck)
    {
        waitingForAck       = true;
        pendingAckCommand   = command;
        pendingAckRequestId = uuidStr;
        ackWaitStartTime    = millis();
        logger.logf("Waiting for acknowledgment for command %d with request ID %s", command,
                    uuidStr.c_str());
    }

    webSocket.sendTXT(jsonPayload);
}

void ElegooCC::connect()
{
    if (webSocket.isConnected())
    {
        webSocket.disconnect();
    }
    webSocket.setReconnectInterval(3000);
    ipAddress = settingsManager.getElegooIP();
    logger.logf("Attempting connection to Elegoo CC @ %s", ipAddress.c_str());
    webSocket.begin(ipAddress, CARBON_CENTAURI_PORT, "/websocket");
}

void ElegooCC::loop()
{
    unsigned long currentTime = millis();

    // websocket IP changed, reconnect
    if (ipAddress != settingsManager.getElegooIP())
    {
        connect();  // this will reconnnect if already connected
    }

    if (webSocket.isConnected())
    {
        // Check for acknowledgment timeout (5 seconds)
        // TODO: need to check the actual requestId
        if (waitingForAck && (currentTime - ackWaitStartTime) >= ACK_TIMEOUT_MS)
        {
            logger.logf("Acknowledgment timeout for command %d, resetting ack state",
                        pendingAckCommand);
            waitingForAck       = false;
            pendingAckCommand   = -1;
            pendingAckRequestId = "";
            ackWaitStartTime    = 0;
        }
        else if (currentTime - lastPing > 29900)
        {
            if (settingsManager.getVerboseLogging())
            {
                logger.log("Sending Ping");
            }
            // For all who venture to this line of code wondering why I didn't use sendPing(), it's
            // because for some reason that doesn't work. but this does!
            this->webSocket.sendTXT("ping");
            lastPing = currentTime;
        }
    }

    // Update expected filament feed if the printer is reporting it
    updateExpectedFilament(currentTime);

    // Before determining if we should pause, check if the filament is moving or it ran out
    checkFilamentMovement(currentTime);
    checkFilamentRunout(currentTime);

    // Check if we should pause the print
    if (shouldPausePrint(currentTime))
    {
        logger.log("Pausing print, detected filament runout or stopped");
        pausePrint();
    }

    webSocket.loop();
}

void ElegooCC::checkFilamentRunout(unsigned long currentTime)
{
    // The signal output of the switch sensor is at low level when no filament is detected
    bool newFilamentRunout = digitalRead(FILAMENT_RUNOUT_PIN) == LOW;
    if (newFilamentRunout != filamentRunout)
    {
        logger.log(filamentRunout ? "Filament has run out" : "Filament has been detected");
    }
    filamentRunout = newFilamentRunout;
}

void ElegooCC::checkFilamentMovement(unsigned long currentTime)
{
    int  currentMovementValue = digitalRead(MOVEMENT_SENSOR_PIN);
    bool debugFlow            = settingsManager.getVerboseLogging();
    bool summaryFlow          = settingsManager.getFlowSummaryLogging();

    // Track movement pulses so we know how much filament actually moved
    if (currentMovementValue != lastMovementValue)
    {
        if (lastMovementValue != -1)
        {
            actualFilamentMM += MOVEMENT_MM_PER_TOGGLE;
            flowTracker.addActual(MOVEMENT_MM_PER_TOGGLE);
            movementPulseCount++;

            if (debugFlow)
            {
                logger.logf("Flow debug: movement pulse (value %d -> %d), pulses=%lu, "
                            "actual=%.3fmm",
                            lastMovementValue, currentMovementValue, movementPulseCount,
                            actualFilamentMM);
            }
        }

        lastMovementValue = currentMovementValue;
        lastChangeTime    = currentTime;
    }

    // If we don't have expected telemetry from SDCP, don't attempt movement-only detection.
    // FilamentStopped should only be derived from SDCP extrusion data.
    if (!expectedTelemetryAvailable)
    {
        currentDeficitMm   = 0.0f;
        deficitThresholdMm = 0.0f;
        deficitRatio       = 0.0f;
        if (filamentStopped)
        {
            logger.log("Filament movement started");
        }
        filamentStopped = false;
        return;
    }

    float deficit          = 0;
    bool  deficitTriggered = false;
    float threshold        = settingsManager.getExpectedDeficitMM();
    if (threshold <= 0)
    {
        threshold = DEFAULT_FILAMENT_DEFICIT_THRESHOLD_MM;
    }
    unsigned long holdMs = settingsManager.getExpectedFlowWindowMs();
    if (holdMs == 0)
    {
        holdMs = EXPECTED_FILAMENT_STALE_MS;
    }
    unsigned long pruneWindow;
    if (settingsManager.getKeepExpectedForever())
    {
        // Effectively disable time-based pruning for expected filament.
        pruneWindow = 0xFFFFFFFFUL;
    }
    else
    {
        pruneWindow = holdMs * 2;
    }

    deficit = flowTracker.outstanding(currentTime, pruneWindow);
    if (deficit < 0)
    {
        deficit = 0;
    }
    deficitTriggered = deficit >= threshold;

    bool deficitHoldSatisfied =
        flowTracker.deficitSatisfied(deficit, currentTime, threshold, holdMs);

    currentDeficitMm   = deficit;
    deficitThresholdMm = threshold;
    deficitRatio       = (threshold > 0.0f) ? (deficit / threshold) : 0.0f;

    if (debugFlow && (currentTime - lastFlowLogMs) >= EXPECTED_FILAMENT_SAMPLE_MS)
    {
        lastFlowLogMs = currentTime;
        logger.logf(
            "Flow debug: cycle tele=%d expected=%.3fmm actual=%.3fmm deficit=%.3fmm "
            "threshold=%.3fmm ratio=%.2f pulses=%lu",
            expectedTelemetryAvailable ? 1 : 0, expectedFilamentMM, actualFilamentMM,
            currentDeficitMm, deficitThresholdMm, deficitRatio, movementPulseCount);
    }

    // Optional condensed logging mode: one summary line per second, even when full
    // verbose logging is disabled. Designed to make long-run debugging easier.
    if (summaryFlow && !debugFlow && (currentTime - lastSummaryLogMs) >= 1000)
    {
        lastSummaryLogMs = currentTime;
        logger.logf("Flow summary: tele=%d expected=%.3fmm actual=%.3fmm deficit=%.3fmm "
                    "threshold=%.3fmm ratio=%.2f pulses=%lu",
                    expectedTelemetryAvailable ? 1 : 0, expectedFilamentMM, actualFilamentMM,
                    currentDeficitMm, deficitThresholdMm, deficitRatio, movementPulseCount);
    }

    bool newFilamentStopped = deficitHoldSatisfied;

    if (newFilamentStopped && !filamentStopped)
    {
        if (deficitTriggered)
        {
            logger.logf(
                "Filament deficit detected (outstanding %.2fmm, threshold %.2fmm, hold %lums, last "
                "delta %.2fmm)",
                deficit, threshold, holdMs, lastExpectedDeltaMM);
        }
        else
        {
            logger.logf("Filament movement stopped, last movement detected %dms ago",
                        currentTime - lastChangeTime);
        }
    }
    else if (!newFilamentStopped && filamentStopped)
    {
        logger.log("Filament movement started");
    }

    filamentStopped = newFilamentStopped;
}

bool ElegooCC::shouldPausePrint(unsigned long currentTime)
{
    if (!settingsManager.getEnabled())
    {
        return false;
    }

    if (filamentRunout && !settingsManager.getPauseOnRunout())
    {
        // if pause on runout is disabled, and filament ran out, skip checking everything else
        // this should let the carbon take care of itself
        return false;
    }

    bool pauseCondition = filamentRunout || filamentStopped;

    bool           sdcpLoss      = false;
    unsigned long  lastSuccessMs = lastSuccessfulTelemetryMs;
    int            lossBehavior  = settingsManager.getSdcpLossBehavior();
    if (webSocket.isConnected() && isPrinting() && lastSuccessMs > 0 &&
        (currentTime - lastSuccessMs) > SDCP_LOSS_TIMEOUT_MS)
    {
        sdcpLoss = true;
    }

    if (sdcpLoss)
    {
        if (lossBehavior == 1)
        {
            pauseCondition = true;
        }
        else if (lossBehavior == 2)
        {
            pauseCondition = false;
        }
    }

    if (currentTime - startedAt < settingsManager.getStartPrintTimeout() ||
        !webSocket.isConnected() || waitingForAck || !isPrinting() ||
        (totalTicks - currentTicks) < 100 || !pauseCondition ||
        (lastPauseRequestMs != 0 && (currentTime - lastPauseRequestMs) < PAUSE_REARM_DELAY_MS))
    {
        return false;
    }

    // log why we paused...
    logger.logf("Pause condition: %d", pauseCondition);
    logger.logf("Filament runout: %d", filamentRunout);
    logger.logf("Filament runout pause enabled: %d", settingsManager.getPauseOnRunout());
    logger.logf("Filament stopped: %d", filamentStopped);
    logger.logf("Time since print start %d", currentTime - startedAt);
    logger.logf("Is Machine status printing?: %d", hasMachineStatus(SDCP_MACHINE_STATUS_PRINTING));
    logger.logf("Print status: %d", printStatus);
    if (settingsManager.getVerboseLogging())
    {
        logger.logf("Flow state: expected=%.3fmm actual=%.3fmm deficit=%.3fmm "
                    "threshold=%.3fmm ratio=%.2f pulses=%lu",
                    expectedFilamentMM, actualFilamentMM, currentDeficitMm,
                    deficitThresholdMm, deficitRatio, movementPulseCount);
    }

    return true;
}

bool ElegooCC::isPrinting()
{
    return printStatus == SDCP_PRINT_STATUS_PRINTING &&
           hasMachineStatus(SDCP_MACHINE_STATUS_PRINTING);
}

// Helper methods for machine status bitmask
bool ElegooCC::hasMachineStatus(sdcp_machine_status_t status)
{
    return (machineStatusMask & (1 << status)) != 0;
}

void ElegooCC::setMachineStatuses(const int *statusArray, int arraySize)
{
    machineStatusMask = 0;  // Clear all statuses first
    for (int i = 0; i < arraySize; i++)
    {
        if (statusArray[i] >= 0 && statusArray[i] <= 4)
        {  // Validate range
            machineStatusMask |= (1 << statusArray[i]);
        }
    }
}

// Get current printer information
printer_info_t ElegooCC::getCurrentInformation()
{
    printer_info_t info;

    info.filamentStopped      = filamentStopped;
    info.filamentRunout       = filamentRunout;
    info.mainboardID          = mainboardID;
    info.printStatus          = printStatus;
    info.isPrinting           = isPrinting();
    info.currentLayer         = currentLayer;
    info.totalLayer           = totalLayer;
    info.progress             = progress;
    info.currentTicks         = currentTicks;
    info.totalTicks           = totalTicks;
    info.PrintSpeedPct        = PrintSpeedPct;
    info.isWebsocketConnected = webSocket.isConnected();
    info.currentZ             = currentZ;
    info.waitingForAck        = waitingForAck;
    info.expectedFilamentMM   = expectedFilamentMM;
    info.actualFilamentMM     = actualFilamentMM;
    info.lastExpectedDeltaMM  = lastExpectedDeltaMM;
    info.telemetryAvailable   = telemetryAvailableLastStatus;
    // Expose deficit metrics for UI/debugging
    info.currentDeficitMm     = currentDeficitMm;
    info.deficitThresholdMm   = deficitThresholdMm;
    info.deficitRatio         = deficitRatio;
    info.movementPulseCount   = movementPulseCount;

    return info;
}

bool ElegooCC::discoverPrinterIP(String &outIp, unsigned long timeoutMs)
{
    WiFiUDP udp;
    if (!udp.begin(SDCP_DISCOVERY_PORT))
    {
        logger.log("Failed to open UDP socket for discovery");
        return false;
    }

    // Use subnet-based broadcast rather than 255.255.255.255 to be friendlier
    // to routers that filter global broadcast.
    IPAddress localIp   = WiFi.localIP();
    IPAddress subnet    = WiFi.subnetMask();
    IPAddress broadcastIp((localIp[0] & subnet[0]) | ~subnet[0],
                          (localIp[1] & subnet[1]) | ~subnet[1],
                          (localIp[2] & subnet[2]) | ~subnet[2],
                          (localIp[3] & subnet[3]) | ~subnet[3]);

    logger.logf("Sending SDCP discovery probe to %s", broadcastIp.toString().c_str());

    udp.beginPacket(broadcastIp, SDCP_DISCOVERY_PORT);
    udp.write(reinterpret_cast<const uint8_t *>("M99999"), 6);
    udp.endPacket();

    unsigned long start = millis();
    while ((millis() - start) < timeoutMs)
    {
        int packetSize = udp.parsePacket();
        if (packetSize > 0)
        {
            IPAddress remoteIp = udp.remoteIP();
            if (remoteIp)
            {
                // Optional: read and log the payload for debugging
                char buffer[128];
                int  len = udp.read(buffer, sizeof(buffer) - 1);
                if (len > 0)
                {
                    buffer[len] = '\0';
                    logger.logf("Discovery reply from %s: %s", remoteIp.toString().c_str(),
                                buffer);
                }
                else
                {
                    logger.logf("Discovery reply from %s (no payload)",
                                remoteIp.toString().c_str());
                }

                outIp = remoteIp.toString();
                udp.stop();
                return true;
            }
        }
        delay(10);
    }

    udp.stop();
    return false;
}
