// OS homework1
// 10627121

// Disable Visual C++ warning for sprintf...
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <thread>
#include <sys/stat.h> // stat()
#include <algorithm>  // std::swap
#include <vector>
#include <time.h>

// Windows-dependent include file
#include <windows.h>
#include <ShellAPI.h>
#pragma comment(lib, "shlwapi.lib")

using namespace std;

enum Mode{
    MODE_UNSET             = 0,
    MODE_BUBBLE            = 1,
    MODE_MULTITHREADING    = 2,
    MODE_MULTIPROCESSING   = 3,
    MODE_BUBBLE_WITH_MERGE = 4,
    MODE_MERGE             = 5
};

typedef char Str100[100];

/* Cross-Process global variable*/

#ifndef __GNUC__
#define SHARED_SECTION
#else
#define SHARED_SECTION __attribute__((section("SHARED"), shared))
#endif

#ifndef __GNUC__
#pragma data_seg("SHARED")
#endif

// !!! Must set initial values, otherwise it will not be put in shared section.
SHARED_SECTION static int uNumArray[1000000] = { 0 };
SHARED_SECTION static int uNumArrayLen = 0;
SHARED_SECTION static int uTotalChildProcess = 0;

#ifndef __GNUC__
#pragma data_seg()
#pragma comment(linker, "/section:SHARED,RWS")
#endif

/* In-file global variable*/
const Str100 gQuitKeyWord[] = { "Q", "q", "QUIT", "quit", "Quit" };
const size_t gQuitKeyWordSize = sizeof(gQuitKeyWord) / sizeof(gQuitKeyWord[0]);

Str100 gFileName;
int gMode = MODE_UNSET;
size_t gThreadNum;
size_t gProcessNum;
char *gExecFileName;

// Execute .exe file with parameters setting
// Parameters:
//   psEXEFile: Path of .exe file
//   psParameters: Parameters of .exe file. NULL for no parameters
//   psInitialDir: Working directory. NULL for current directory
//   bShowWindow:  Show running window. true for yes, false for no
//   bWaitProcessFinish: Wait child process finished. true for yes, false for no
// Return: Process handler
HANDLE ExecuteFile(char *psEXEFile, char *psParameters, char *psInitialDir, bool bShowWindow, bool bWaitProcessFinish)
{
    SHELLEXECUTEINFO ShExecInfo;

    memset(&ShExecInfo, 0, sizeof(ShExecInfo));
    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.hwnd = NULL;
    ShExecInfo.lpVerb = NULL;
    ShExecInfo.lpFile = psEXEFile;
    ShExecInfo.lpParameters = psParameters;
    ShExecInfo.lpDirectory = psInitialDir;
    ShExecInfo.nShow = bShowWindow ? SW_SHOW : SW_HIDE;
    ShExecInfo.hInstApp = NULL;
    if (ShellExecuteEx(&ShExecInfo))
        if (bWaitProcessFinish)
            WaitForSingleObject(ShExecInfo.hProcess, INFINITE); // Wait process finish

    return ShExecInfo.hProcess;
}

// Get one line input string
// Parameters:
//   str: Variable that store input string
//   size: Size of str
// Return: status. 1 for get one input string, EOF for end of file
int InputStr(char *str, int size) {
    // Get entire line input
    if (fgets(str, size, stdin) == NULL)
        return EOF;

    if (str[strlen(str) - 1] == '\n')
        str[strlen(str) - 1] = '\0';

    return 1;
} // InputStr()

// 檔案是否存在
bool FileExist(char *pFileName) {
    struct stat st;

    return (stat(pFileName, &st) == 0); // stat回傳零表示檔案存在
} // FileExist()

bool ReadFile() {
    bool ret = true;

    // Open file
    FILE * fp = fopen(gFileName, "rt");

    // Check and set mode
    fscanf(fp, "%d", &gMode);
    if (gMode < 1 || gMode > 4) {
        printf("Error: No such mode %d.\n", gMode);
        ret = false;
    } // if
    else { // Read data
        int num;
        uNumArrayLen = 0;
        while (fscanf(fp, "%d", &num) != EOF)
            uNumArray[uNumArrayLen++] = num;
    } // else

    // Close file
    fclose(fp);

    return ret;
} // ReadFile()

bool Input(int argc, char* argv[]) {
    Str100 segmentNumStr;

    // Get file name
    gFileName[0] = NULL;
    while (gFileName[0] == NULL) {
        printf("請輸入檔案名稱：");

        // Input file name
        if (InputStr(gFileName, sizeof(gFileName)) == EOF) return false;
        
        // Want quit?
        for (int i = 0; i < gQuitKeyWordSize; i++)
            if (strcmp(gFileName, gQuitKeyWord[i]) == 0) return false;

        // Check file name
        if (!FileExist(gFileName)) {
            printf("Error: No such file.\n");
            gFileName[0] = NULL;
        } // if
        else if (!ReadFile())
            gFileName[0] = NULL;
    } // while

    // Get segment amount
    if (gMode == MODE_BUBBLE) uTotalChildProcess = 1;
    else {
        uTotalChildProcess = 0;
        while (!uTotalChildProcess) {
            printf("請輸入要切成幾份：");

            // Input segment amount
            if (InputStr(segmentNumStr, sizeof(segmentNumStr)) == EOF) return false;

            // Want quit?
            for (int i = 0; i < gQuitKeyWordSize; i++)
                if (strcmp(segmentNumStr, gQuitKeyWord[i]) == 0) return false;

            // Check segment amount
            uTotalChildProcess = atoi(segmentNumStr);
            if (uTotalChildProcess <= 0 || uTotalChildProcess > uNumArrayLen) {
                printf("Error: No enough data to partition.\n");
                uTotalChildProcess = 0;
            } // if
        } // while
    } // else

    return true;
} // Input()

void BubbleSort(int start, int end) {
    for (int swapNum = 1; end > start && swapNum != 0; end--) {
        swapNum = 0;
        // i: start ~ end - 1
        for (int i = start; i < end; i++)
            // Need swap?
            if (uNumArray[i] > uNumArray[i + 1]) {
                swap(uNumArray[i], uNumArray[i + 1]);
                swapNum++;
            } // if
    } // for
} // BubbleSort()

void Merge(int start, int mid, int end) {
    size_t leftSize  = mid - start + 1;  // Size of left subArray
    size_t rightSize = end - mid;        // Size of right subArray
    size_t size = leftSize + rightSize;  // Size of mergedArray
    int *mergedNumArray = new int[size]; // Allocate memory of mergedNumArray

    // Merge two subArray
    for (int i = 0, leftIndex = start, rightIndex = mid + 1; i < size; i++)
        mergedNumArray[i] = uNumArray[(rightIndex > end ||
        (leftIndex <= mid && uNumArray[leftIndex] <= uNumArray[rightIndex])) ?
        leftIndex++ : rightIndex++];

    // Copy mergedNumArray to numArray
    for (int i = 0; i < size; i++)
        uNumArray[start + i] = mergedNumArray[i];

    // Free memory of mergedNumArray
    delete mergedNumArray;
} // Merge()

void MergeSort(int startSeg, int endSeg) {
    char buf[100];
    int firstIndex = uNumArrayLen * startSeg / uTotalChildProcess;
    int lastIndex = uNumArrayLen * (endSeg + 1) / uTotalChildProcess - 1;
    if (startSeg == endSeg) return;
    else {
        int midSeg = (startSeg + endSeg) >> 1;
        MergeSort(startSeg, midSeg);
        MergeSort(midSeg + 1, endSeg);
        if (gMode == MODE_MULTITHREADING) {
            thread mergeThread(Merge, firstIndex, uNumArrayLen * (midSeg+1) / uTotalChildProcess - 1, lastIndex);
            gThreadNum++;       
            mergeThread.join();
        } // if
        else if (gMode == MODE_MULTIPROCESSING) {
            sprintf(buf, "0 %d %d %d", MODE_MERGE, startSeg, endSeg);
            ///printf("buf = [%s]\n", buf);
            ExecuteFile(gExecFileName, buf, NULL, false, true);
            gProcessNum++;
        } // else if
        else Merge(firstIndex, uNumArrayLen * (midSeg + 1) / uTotalChildProcess - 1, lastIndex);
    } // else
} // MergeSort()

void Output(clock_t startTime) {
    // Set output file name
    gFileName[strlen(gFileName) - 4] = '\0';
    strcat(gFileName, "_output.txt");

    // Open file
    FILE * fp = fopen(gFileName, "wt");

    // Write file
    fprintf(fp, "排序：\n");
    for (int i = 0, size = uNumArrayLen; i < size; i++)
        fprintf(fp, "%d ", uNumArray[i]);

    fprintf(fp, "\n執行時間： %f\n", (clock() - startTime) / (double)(CLOCKS_PER_SEC));

    // Close file
    fclose(fp);

    // Screen output
    printf("Total threads:  %d\n", gThreadNum);
    printf("Total processs: %d\n", gProcessNum);
} // Output

int main(int argc, char* argv[]) {
    bool isQuit = false;
    clock_t startTime;
    Str100 roleIDStr;
    HANDLE *processArray;
    thread *threadArray;
    int roleID;
    
    // Test data
#if 0
    static char *newargv[] = { "thread.exe", "-1", "3"};
    argc = 3;
    argv = newargv;
#endif

    // Check command line parameter
    gExecFileName = argv[0];
    if (argc < 2) {
        fprintf(stderr, "usage: %s roleID\n"
            "roleID --- -1: Main process; other: child process\n", gExecFileName);
        return -1;
    } // if
    roleID = atoi(argv[1]);
    gMode = (argc >= 3) ? atoi(argv[2]) : MODE_UNSET;

    // Child process
    if (roleID != -1) {
        if (uNumArrayLen == 0) {
            fprintf(stderr, "Main process need execute first!\n");
            return -1;
        } // if
        printf("[%s][%s]!\n", argv[3], argv[4]);
        int startSeg = atoi(argv[3]);
        int endSeg = atoi(argv[4]);
        int midSeg = (startSeg + endSeg) >> 1;
        int firstIndex = uNumArrayLen * startSeg / uTotalChildProcess;
        int lastIndex = uNumArrayLen * (endSeg + 1) / uTotalChildProcess - 1;
        if (gMode == MODE_BUBBLE)
            BubbleSort(firstIndex, lastIndex);
        else if (gMode == MODE_MERGE)
            Merge(firstIndex, uNumArrayLen * (midSeg + 1) / uTotalChildProcess - 1, lastIndex);
        else {
            fprintf(stderr, "Error: No such mode %d\n", gMode);
            return -1;
        } // else
    } // else
    
    // Main thread/Process
    while (roleID == -1 && !isQuit) {
        // Has input?
        if (Input(argc, argv)) {
            // Set timer
            startTime = clock();

            // Allocate process & thread array
            processArray = new HANDLE[uTotalChildProcess];
            threadArray  = new thread[uTotalChildProcess];

            // Sorting
            gThreadNum = gProcessNum = 0;
            if (uNumArrayLen != 0) {
                ///printf("%d %d\n", i, size);
                bool showCmdWindow = FileExist("ShowCmdWindow");
                for (int i = 0, startIndex = 0, endIndex = 0; i < uTotalChildProcess; i++) {
                    startIndex = uNumArrayLen * i       / uTotalChildProcess;
                    endIndex   = uNumArrayLen * (i + 1) / uTotalChildProcess - 1;
                    if (gMode == MODE_MULTITHREADING) {
                        threadArray[i] = thread(BubbleSort, startIndex, endIndex);
                        gThreadNum++;
                    } // else
                    else if (gMode == MODE_MULTIPROCESSING) {
                        sprintf(roleIDStr, "%d %d %d %d", i, MODE_BUBBLE, i, i);
                        processArray[i] = ExecuteFile(gExecFileName, roleIDStr, NULL, showCmdWindow, false);
                        gProcessNum++;
                    } // else if
                    else BubbleSort(startIndex, endIndex);
                } // for

                // Wait bubble sort complete
                if (gMode == MODE_MULTITHREADING)
                    for (int i = 0; i < uTotalChildProcess; i++)
                        threadArray[i].join();
                else if (gMode == MODE_MULTIPROCESSING)
                    WaitForMultipleObjects(uTotalChildProcess, processArray, true, INFINITE); // Wait all processes finish

                if (gMode != MODE_BUBBLE)
                    MergeSort(0, uTotalChildProcess - 1);
            } // if

            // Free process & thread array
            delete[] processArray;
            delete[] threadArray;

            // Output
            Output(startTime);
            ///isQuit = true;
        } // if
        else isQuit = true;
    } // while

    return 0;
} // main()
