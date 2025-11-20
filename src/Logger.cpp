#include "Logger.h"
#include "time.h"

// External function to get current time (from main.cpp)
extern unsigned long getTime();

Logger &Logger::getInstance()
{
  static Logger instance;
  return instance;
}

Logger::Logger()
{
  currentIndex = 0;
  totalEntries = 0;
  logCapacity = MAX_LOG_ENTRIES;
  uuidGenerator.generate();
  logBuffer = new (std::nothrow) LogEntry[logCapacity];
  if (!logBuffer)
  {
    logCapacity = FALLBACK_LOG_ENTRIES;
    logBuffer  = new (std::nothrow) LogEntry[logCapacity];
    if (!logBuffer)
    {
      logCapacity = 0;
    }
  }
}

Logger::~Logger()
{
  delete[] logBuffer;
}

void Logger::log(const String &message)
{
  // Print to serial first
  Serial.println(message);

  // Generate UUID for this log entry
  uuidGenerator.generate();
  String uuid = String(uuidGenerator.toCharArray());

  // Get current timestamp
  unsigned long timestamp = getTime();

  if (logCapacity == 0 || logBuffer == nullptr)
  {
    return;
  }

  // Store in circular buffer
  logBuffer[currentIndex].uuid = uuid;
  logBuffer[currentIndex].timestamp = timestamp;
  logBuffer[currentIndex].message = message;

  // Update indices
  currentIndex = (currentIndex + 1) % logCapacity;
  if (totalEntries < logCapacity)
  {
    totalEntries++;
  }
}

void Logger::log(const char *message)
{
  log(String(message));
}

void Logger::logf(const char *format, ...)
{
  char buffer[512];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(String(buffer));
}

String Logger::getLogsAsJson()
{
  DynamicJsonDocument jsonDoc(32768); // Allocate space for expanded log buffer
  JsonArray logsArray = jsonDoc.createNestedArray("logs");

  if (logCapacity == 0 || logBuffer == nullptr)
  {
    String jsonResponse;
    serializeJson(jsonDoc, jsonResponse);
    return jsonResponse;
  }

  // If we have less than MAX_LOG_ENTRIES, start from 0
  // Otherwise, start from currentIndex (oldest entry)
  int startIndex = (totalEntries < logCapacity) ? 0 : currentIndex;
  int count = totalEntries;

  for (int i = 0; i < count; i++)
  {
    int bufferIndex = (startIndex + i) % MAX_LOG_ENTRIES;

    JsonObject logEntry = logsArray.createNestedObject();
    logEntry["uuid"] = logBuffer[bufferIndex].uuid;
    logEntry["timestamp"] = logBuffer[bufferIndex].timestamp;
    logEntry["message"] = logBuffer[bufferIndex].message;
  }

  String jsonResponse;
  serializeJson(jsonDoc, jsonResponse);
  return jsonResponse;
}

String Logger::getLogsAsText()
{
  String result;

  if (logCapacity == 0 || logBuffer == nullptr)
  {
    return result;
  }

  // If we have less than MAX_LOG_ENTRIES, start from 0
  // Otherwise, start from currentIndex (oldest entry)
  int startIndex = (totalEntries < logCapacity) ? 0 : currentIndex;
  int count      = totalEntries;

  for (int i = 0; i < count; i++)
  {
    int bufferIndex = (startIndex + i) % MAX_LOG_ENTRIES;

    result += String(logBuffer[bufferIndex].timestamp);
    result += " ";
    result += logBuffer[bufferIndex].message;
    result += "\n";
  }

  return result;
}

void Logger::clearLogs()
{
  currentIndex = 0;
  totalEntries = 0;
  if (logCapacity == 0 || logBuffer == nullptr)
  {
    return;
  }
  // Clear the buffer
  for (int i = 0; i < logCapacity; i++)
  {
    logBuffer[i].uuid = "";
    logBuffer[i].timestamp = 0;
    logBuffer[i].message = "";
  }
}

int Logger::getLogCount()
{
  return totalEntries;
}
