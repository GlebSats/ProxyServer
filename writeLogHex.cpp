#include "writeLogHex.h"

void writeLogHex(std::string message, char first, char second, char third)
{
	try
	{
		//Find path to folder
		TCHAR Path[MAX_PATH];
		GetModuleFileName(NULL, Path, MAX_PATH);
		std::wstring logPath = Path;
		size_t i = logPath.find_last_of(L"\\");
		logPath = logPath.substr(0, i + 1) + L"log.txt";

		// Open file
		std::fstream file;
		file.open(logPath, std::ios::app);

		// Timing
		std::time_t currentTime = std::time(nullptr);
		std::tm localTime;
		localtime_s(&localTime, &currentTime);
		char dateTime[20];
		std::strftime(dateTime, sizeof(dateTime), "%d-%m-%Y %H:%M:%S", &localTime);

		// Write log

		file << "[" << dateTime << "] " << message << hex << static_cast<int>(first) << " " << static_cast<int>(second) << " " << static_cast<int>(third) << std::endl;

		file.close();
	}
	catch (const std::exception&)
	{

	}
}
