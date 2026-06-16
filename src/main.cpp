
#ifdef __TEST_MAIN__
#include "test-main.h"
#endif


#include "quality-report.h"
#include <string>
#include <vector>

using namespace std;
using namespace reports;

#if defined(__TEST_MAIN__) || defined(__CLI_MAIN__) 

int main(int argc, char* argv[]) {
#if defined(__TEST_MAIN__)
    test_main(argc, argv);    
#elif defined(__CLI_MAIN__) 
    vector<string> files = {
        "AIA.csv",
        "BBUS.csv",
        "DLN.csv",
        "EMHY.csv",
        "FNDX.csv",
        "FTXL.csv",
        "GOEX.csv",
        "IAU.csv",
        "IWY.csv",
        "IYW.csv",
        "JMOM.csv",
        "LIT.csv",
        "QTUM.csv",
        "SJNK.csv",
        "SMMD.csv",
        "SOXX.csv",
        "SPMO.csv",
        "SPYV.csv",
        "VBK.csv",
        "VGT.csv",
        "VONG.csv",
        "XLF.csv",
        "XLU.csv"

    };
    quality(argv[0], files);
#endif

}
#endif 
