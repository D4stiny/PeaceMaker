// PeaceMaker CLI.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <iostream>
#include <string>
#include "IOCTLCommunicationUser.h"

/**
	Print stack history.
	@param StackHistory - The history to display.
	@param StackHistorySize - The size of the StackHistory array.
*/
VOID
DisplayStackHistory(
	_In_ PSTACK_RETURN_INFO StackHistory,
	_In_ ULONG StackHistorySize
	)
{
	ULONG i;

	printf("DisplayStackHistory: \tStack History:\n");
	for (i = 0; i < StackHistorySize; i++)
	{
		if (StackHistory[i].MemoryInModule)
		{
			printf("DisplayStackHistory: \t\t%ws+0x%X\n", StackHistory[i].BinaryPath, StackHistory[i].BinaryOffset);
		}
		else
		{
			printf("DisplayStackHistory: \t\t0x%llx (Manual Mapped)\n", StackHistory[i].RawAddress);
		}
	}
}

/**
	Display an alert. Supports the various alert types.
	@param Alert - Alert to display.
*/
VOID
DisplayAlert (
	PBASE_ALERT_INFO Alert
	)
{
	PSTACK_VIOLATION_ALERT stackViolationAlert;
	PFILTER_VIOLATION_ALERT filterViolationAlert;

	//
	// Sanity check.
	//
	if (Alert == NULL)
	{
		printf("DisplayAlert: No alert found.\n");
		return;
	}

	printf("DisplayAlert: Alert dump:\n");
	switch (Alert->AlertSource)
	{
	case ProcessCreate:
		printf("DisplayAlert: \tSource: Process creation callback\n");
		break;
	case ImageLoad:
		printf("DisplayAlert: \tSource: Image load callback\n");
		break;
	case RegistryFilterMatch:
		printf("DisplayAlert: \tSource: Registry filter match\n");
		break;
	case FileFilterMatch:
		printf("DisplayAlert: \tSource: File filter match\n");
		break;
	case ThreadCreate:
		printf("DisplayAlert: \tSource: Thread creation callback\n");
		break;
	}
	
	switch (Alert->AlertType)
	{
	case StackViolation:
		stackViolationAlert = RCAST<PSTACK_VIOLATION_ALERT>(Alert);
		printf("DisplayAlert: \tAlert Type: Stack walk violation\n");
		printf("DisplayAlert: \tViolating Address: 0x%llx\n", stackViolationAlert->ViolatingAddress);
		DisplayStackHistory(stackViolationAlert->StackHistory, stackViolationAlert->StackHistorySize);
		break;
	case FilterViolation:
		filterViolationAlert = RCAST<PFILTER_VIOLATION_ALERT>(Alert);
		printf("DisplayAlert: \tAlert Type: Filter violation\n");
		printf("DisplayAlert: \tFilter content: %ws\n", filterViolationAlert->ViolatedFilter.MatchString);
		printf("DisplayAlert: \tFilter flags: 0x%X\n", filterViolationAlert->ViolatedFilter.Flags);
		DisplayStackHistory(filterViolationAlert->StackHistory, filterViolationAlert->StackHistorySize);
		break;
	}
}

int main()
{
	IOCTLCommunication communicator;
	std::string input;
	int choice;

	PBASE_ALERT_INFO alert;
	PPROCESS_SUMMARY_REQUEST processSummaries;
	PPROCESS_DETAILED_REQUEST processDetailed;
	PROCESS_SIZES_REQUEST processSizes;
	LIST_FILTERS_REQUEST filters;
	PIMAGE_DETAILED_REQUEST imageDetailed;

	ULONG i;
	int skipCount;
	int requestCount;
	HANDLE processID;
	ULONG executionTime;
	STRING_FILTER_TYPE filterType;
	ULONG filterFlags;
	std::wstring filterContent;
	ULONG filterId;
	ULONG imageIndex;

	choice = 7;

	printf("main: Initiating communication with the driver.\n");
	if (communicator.ConnectDevice() == FALSE)
	{
		printf("main: Failed to connect to the device.\n");
		goto Exit;
	}

	printf("main: Communication initiated.\n");
	do
	{
		printf("main: PeaceMaker basic CLI utility\n");
		printf("main: 1. Check if there are alerts queued.\n");
		printf("main: 2. Pop an alert from the list of alerts.\n");
		printf("main: 3. Request a list of process summaries.\n");
		printf("main: 4. Request detailed information on a process.\n");
		printf("main: 5. Request to add a filter.\n");
		printf("main: 6. Request a list of filters.\n");
		printf("main: 7. Request detailed information on an image in a process.\n");
		printf("main: 8. Exit.\n");

		std::cin >> input;
		choice = std::stoi(input);

		//
		// Depending on the user's choice, dispatch to the correct function.
		//
		switch (choice)
		{
		case 1:
			if (communicator.QueuedAlerts())
			{
				printf("main: There are alerts queued.\n");
				break;
			}
			printf("main: There are no alerts queued.\n");
			break;
		case 2:
			alert = communicator.PopAlert();
			DisplayAlert(alert);
			free(alert);
			alert = NULL;
			break;
		case 3:
			//
			// Ask for the necessary information.
			//
			printf("main: How many processes should we skip?\n");
			std::cin >> input;
			skipCount = std::stoi(input);

			printf("main: How many processes should we request?\n");
			std::cin >> input;
			requestCount = std::stoi(input);

			//
			// Get the summamries.
			//
			processSummaries = communicator.RequestProcessSummary(skipCount, requestCount);
			if (processSummaries == NULL)
			{
				printf("main: Failed to retrieve process summaries.\n");
				break;
			}

			//
			// Print the processes in a "table" format.
			//
			printf("main: %-10s\t%-50s\t%-12s\n", "Process ID", "Path", "Execution Time");
			for (i = 0; i < processSummaries->ProcessHistorySize; i++)
			{
				if (processSummaries->ProcessHistory[i].ProcessId == 0)
				{
					continue;
				}
				printf("main: %-10i\t%-50ws\t%-12i\n", processSummaries->ProcessHistory[i].ProcessId, processSummaries->ProcessHistory[i].ImageFileName, processSummaries->ProcessHistory[i].EpochExecutionTime);
			}

			free(processSummaries);
			processSummaries = NULL;
			break;
		case 4:
			//
			// Ask for the necessary information.
			//
			printf("main: What is the target process ID?\n");
			std::cin >> input;
			processID = RCAST<HANDLE>(std::stoi(input));

			printf("main: What is the processes execution time in epoch?\n");
			std::cin >> input;
			executionTime = std::stoi(input);

			processSizes = communicator.GetProcessSizes(processID, executionTime);
			printf("main: ImageSize = %i, StackSize = %i\n", processSizes.ImageSize, processSizes.StackSize);

			//
			// Request a detailed report on the process.
			//
			processDetailed = communicator.RequestDetailedProcess(processID, executionTime, processSizes.ImageSize, processSizes.StackSize);
			if (processDetailed == NULL || processDetailed->Populated == FALSE)
			{
				printf("main: Failed to retrieve a detailed process report.\n");
				break;
			}

			printf("main: Process 0x%X:\n", processID);
			printf("main: \tProcess Path: %ws\n", processDetailed->ProcessPath);
			printf("main: \tCaller Process ID: 0x%X\n", processDetailed->CallerProcessId);
			printf("main: \tCaller Process Path: %ws\n", processDetailed->CallerProcessPath);
			printf("main: \tParent Process ID: 0x%X\n", processDetailed->ParentProcessId);
			printf("main: \tParent Process Path: %ws\n", processDetailed->ParentProcessPath);
			DisplayStackHistory(processDetailed->StackHistory, processDetailed->StackHistorySize);

			printf("main: \t%-3s\t%-100s:\n", "ID", "IMAGE PATH");
			for (i = 0; i < processDetailed->ImageSummarySize; i++)
			{
				printf("main: \t\t%-3i\t%-100ws\n", i, processDetailed->ImageSummary[i].ImagePath);
			}

			free(processDetailed->ImageSummary);
			free(processDetailed->StackHistory);
			free(processDetailed);
			processDetailed = NULL;
			break;
		case 5:
			//
			// Ask for the necessary information.
			//
			printf("main: What type of filter to add (R/F)?\n");
			std::cin >> input;
			switch (input[0])
			{
			case 'R':
			case 'r':
				filterType = RegistryFilter;
				break;
			case 'F':
			case 'f':
				filterType = FilesystemFilter;
				break;
			}

			printf("main: What flags should the filter have (D/W/E, can enter multiple)?\n");
			std::cin >> input;
			filterFlags = 0;
			for (char c : input)
			{
				switch (c)
				{
				case 'D':
				case 'd':
					filterFlags |= FILTER_FLAG_DELETE;
					break;
				case 'W':
				case 'w':
					filterFlags |= FILTER_FLAG_WRITE;
					break;
				case 'E':
				case 'e':
					filterFlags |= FILTER_FLAG_DELETE;
					break;
				}
			}

			printf("main: What should the filter content be?\n");
			std::wcin >> filterContent;

			filterId = communicator.AddFilter(filterType, filterFlags, CCAST<PWCHAR>(filterContent.c_str()), filterContent.length());
			if (filterId)
			{
				printf("main: Filter added with ID 0x%X.\n", filterId);
				break;
			}
			printf("main: Failed to add filter.\n");
			break;
		case 6:
			//
			// Ask for the necessary information.
			//
			printf("main: What type of filter to list (R/F)?\n");
			std::cin >> input;
			switch (input[0])
			{
			case 'R':
			case 'r':
				filterType = RegistryFilter;
				break;
			case 'F':
			case 'f':
				filterType = FilesystemFilter;
				break;
			}

			printf("main: How many filters should we skip?\n");
			std::cin >> input;
			skipCount = std::stoi(input);

			filters = communicator.RequestFilters(filterType, skipCount);
			for (i = 0; i < filters.CopiedFilters; i++)
			{
				printf("main: Filter 0x%X:\n");
				switch (filters.Filters[i].Type)
				{
				case RegistryFilter:
					printf("main: \tFilter Type: Registry filter");
					break;
				case FilesystemFilter:
					printf("main: \tFilter Type: Filesystem filter");
					break;
				}

				printf("main: \tFilter Flags: ");

				if (FlagOn(filters.Filters[i].Flags, FILTER_FLAG_DELETE))
				{
					printf("main: \t\tDELETE\n");
				}
				if (FlagOn(filters.Filters[i].Flags, FILTER_FLAG_WRITE))
				{
					printf("main: \t\tWRITE\n");
				}
				if (FlagOn(filters.Filters[i].Flags, FILTER_FLAG_EXECUTE))
				{
					printf("main: \t\tDELETE\n");
				}

				printf("main: \tFilter Content: %ws\n", filters.Filters[i].MatchString);
			}
			break;
		case 7:
			//
			// Ask for the necessary information.
			//
			printf("main: What is the target process ID?\n");
			std::cin >> input;
			processID = RCAST<HANDLE>(std::stoi(input));

			printf("main: What is the processes execution time in epoch?\n");
			std::cin >> input;
			executionTime = std::stoi(input);

			processSizes = communicator.GetProcessSizes(processID, executionTime);
			printf("main: ImageSize = %i, StackSize = %i\n", processSizes.ImageSize, processSizes.StackSize);

			//
			// Request a detailed report on the process.
			//
			processDetailed = communicator.RequestDetailedProcess(processID, executionTime, processSizes.ImageSize, processSizes.StackSize);
			if (processDetailed == NULL || processDetailed->Populated == FALSE)
			{
				printf("main: Failed to retrieve a detailed process report.\n");
				break;
			}

			printf("main: \t%-3s\t%-50s\n", "ID", "IMAGE PATH");
			for (i = 0; i < processDetailed->ImageSummarySize; i++)
			{
				printf("main: \t%-3i\t%-50ws\n", i, processDetailed->ImageSummary[i].ImagePath);
			}

			printf("main: Enter the ID of the image to query.\n");
			std::cin >> input;
			imageIndex = std::stoi(input);

			printf("main: Image stack size: %i\n", processDetailed->ImageSummary[imageIndex].StackSize);

			imageDetailed = communicator.RequestDetailedImage(processID, executionTime, imageIndex, processDetailed->ImageSummary[imageIndex].StackSize);
			if (imageDetailed == NULL || imageDetailed->Populated == FALSE)
			{
				printf("main: Failed to retrieve a detailed image report.\n");
				break;
			}

			printf("main: Image %i:\n", imageIndex);
			printf("main: \tPath: %ws\n", imageDetailed->ImagePath);
			DisplayStackHistory(imageDetailed->StackHistory, imageDetailed->StackHistorySize);

			free(processDetailed);
			free(imageDetailed);
			break;
		case 8:
			//
			// No handling required, will exit when the while condition is checked.
			//
			break;
		default:
			printf("main: Unrecognized option %i.\n", choice);
			break;
		}
	} while (input != "8");
Exit:
	_fgetchar();
	return 0;
}