// =======================================================================================
//   PC forwarding: lines from the USB serial port go to the MarcDuinos
// =======================================================================================
// Used by the serial web console (see docs/web-console.md). Turn it on with
// #define ENABLE_PC_TO_MARCDUINO_FORWARDING 1 in a_INIT.ino; with 0 this file adds nothing.
//
// One command per line (115200 baud, ended by \n, \r or \r\n):
//   S1:<command>   sends <command> to the dome MarcDuino (Serial1), e.g. S1::OP01
//   S3:<command>   sends <command> to the body MarcDuino (Serial3)
//   PING           answers PONG
//   HELP           lists these commands
// A \r is added to the command when it does not end with one (MarcDuino syntax).
// Same lines and answers as ShadowMD Lite.
#ifndef PC_FORWARDING_H
#define PC_FORWARDING_H

#if ENABLE_PC_TO_MARCDUINO_FORWARDING

#define PC_FWD_LINE_SIZE 64            // USB line buffer, including the terminating NUL

// One line from the USB serial port, without the line ending. true when the line was
// a forwarding command (S1:, S3:, PING, HELP) and has been answered; false when it is
// not one, so the caller can try its own commands.
bool pcFwdHandleLine(const char* line)
{
  // Skip leading and trailing spaces/tabs.
  while (*line == ' ' || *line == '\t') line++;
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t')) len--;
  if (len == 0) return false;

  if ((len == 4 && strncmp(line, "PING", 4) == 0) || (len == 4 && strncmp(line, "ping", 4) == 0))
  {
    Serial.println(F("PONG"));
    return true;
  }

  if ((len == 4 && strncmp(line, "HELP", 4) == 0) || (len == 4 && strncmp(line, "help", 4) == 0))
  {
    Serial.println(F("Shadow MD PC Serial forwarding ON"));
    Serial.println(F("Use: S1:<COMMAND> or S3:<COMMAND>"));
    Serial.println(F("Example: S1::SE01"));
    return true;
  }

  // S1:<command> or S3:<command>. The command may already begin with ':' (MarcDuino syntax).
  if (len < 3 || !(line[0] == 'S' || line[0] == 's') || !(line[1] == '1' || line[1] == '3') || line[2] != ':')
    return false;

  int targetPort = (line[1] == '1') ? 1 : 3;
  const char* payload = line + 3;
  const char* end = line + len;
  while (payload < end && (*payload == ' ' || *payload == '\t')) payload++;
  size_t n = end - payload;
  if (n == 0)
  {
    Serial.println(F("ERR Empty command payload"));
    return true;
  }

  // Copy the command and make sure it ends with the carriage return MarcDuino expects.
  char cmd[PC_FWD_LINE_SIZE + 1];
  if (n > PC_FWD_LINE_SIZE - 1)
  {
    Serial.println(F("ERR Command too long"));
    return true;
  }
  memcpy(cmd, payload, n);
  if (cmd[n - 1] != '\r') cmd[n++] = '\r';
  cmd[n] = '\0';

  if (targetPort == 1)
    Serial1.print(cmd);
  else
    Serial3.print(cmd);

  Serial.print(F("OK forwarded to "));
  Serial.println(targetPort == 1 ? F("Serial1") : F("Serial3"));
  return true;
}

char    pcFwdLine[PC_FWD_LINE_SIZE];   // USB line being received
uint8_t pcFwdLineLen = 0;
bool    pcFwdLineOverflow = false;     // line longer than the buffer: discard it
bool    pcFwdLineBadByte = false;      // NUL or non-ASCII byte in the line: reject it

// Reads the USB serial port without blocking and hands each complete line (ended by \n or
// \r, so \r\n works too) to pcFwdHandleLine(). A line that does not fit in
// PC_FWD_LINE_SIZE - 1 characters is discarded up to its end and answered with
// "ERR line too long"; a line with a NUL or non-ASCII byte, or one that is not a
// forwarding command, is answered with "ERR unknown command".
// Not used with ENABLE_ROS2D2 1: then rosReadUsb() reads the port and rosHandleLine()
// calls pcFwdHandleLine() first.
void pcFwdReadUsb()
{
  while (Serial.available() > 0)
  {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r')
    {
      if (pcFwdLineOverflow)
      {
        Serial.println(F("ERR line too long"));
      } else if (pcFwdLineBadByte)
      {
        Serial.println(F("ERR unknown command"));
      } else if (pcFwdLineLen > 0)
      {
        pcFwdLine[pcFwdLineLen] = '\0';
        if (!pcFwdHandleLine(pcFwdLine))
        {
          // A line of only spaces is ignored, anything else is not ours.
          const char* p = pcFwdLine;
          while (*p == ' ' || *p == '\t') p++;
          if (*p != '\0') Serial.println(F("ERR unknown command"));
        }
      }
      pcFwdLineLen = 0;
      pcFwdLineOverflow = false;
      pcFwdLineBadByte = false;
      continue;
    }

    if (pcFwdLineOverflow) continue;  // drop the rest of a line that is too long
    if (c == '\0' || (uint8_t)c >= 0x80) pcFwdLineBadByte = true;
    if (pcFwdLineLen < PC_FWD_LINE_SIZE - 1)
    {
      pcFwdLine[pcFwdLineLen++] = c;
    } else
    {
      pcFwdLineOverflow = true;
    }
  }
}

#endif // ENABLE_PC_TO_MARCDUINO_FORWARDING
#endif // PC_FORWARDING_H
