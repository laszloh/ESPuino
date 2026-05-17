#pragma once

constexpr uint8_t ftpUserLength = 10u; // Length will be published n-1 as maxlength to GUI
constexpr uint8_t ftpPasswordLength = 15u; // Length will be published n-1 as maxlength to GUI

#ifdef FTP_ENABLE
consteval bool isFtpCompiled() { return true; }

void Ftp_Init(void);
void Ftp_Cyclic(void);
void Ftp_EnableServer(void);

#else // if FTP_ENABLE not defined, define dummy functions to avoid #ifdefs in code
consteval bool isFtpCompiled() { return false; }

inline void Ftp_Init(void) {}
inline void Ftp_Cyclic(void) {}
inline void Ftp_EnableServer(void) {}

#endif // FTP_ENABLE
