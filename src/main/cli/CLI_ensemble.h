#ifndef __CLI_ENSEMBLED_H__
#define __CLI_ENSEMBLED_H__
#include "nanodbc/nanodbc.h"
#include <unordered_map>
#include <string>
#include "events/SignalEvent.h"
#include "timeseries/timeseries.h"
#include <vector>

using namespace std;
using namespace events;
using namespace timeseries;

/* 
    [
        {
            "timestamp": "2001-12-31T00:00:00.00",
            "entries": {
                "AAPL": {
                    "generator_a": {
                        "filter_a":{
                            "signal": -1,
                            "μ": 0,
                            "σ": 0,
                            "δ": 0,
                            "signal":0,
                            "window_size": 0,
                            "r": 0,
                            "x_min": 0,
                            "x_max": 0
                            "acceleration": 0
                        },
                        "filter_b":{
                            "signal": -1,
                            "μ": 0,
                            "σ": 0,
                            "δ": 0,
                            "signal":0,
                            "window_size": 0,
                            "r": 0,
                            "x_min": 0,
                            "x_max": 0
                            "acceleration": 0                        
                        }                        
                    }
                },
                "MSFT": {
                    "generator_a": {
                        "filter_a":{
                            "signal": -1,
                            "μ": 0,
                            "σ": 0,
                            "δ": 0,
                            "signal":0,
                            "window_size": 0,
                            "r": 0,
                            "x_min": 0,
                            "x_max": 0
                            "acceleration": 0
                        },
                        "filter_b":{
                            "signal": -1,
                            "μ": 0,
                            "σ": 0,
                            "δ": 0,
                            "signal":0,
                            "window_size": 0,
                            "r": 0,
                            "x_min": 0,
                            "x_max": 0
                            "acceleration": 0                        
                        }                        
                    }                
                }
            }
        }
    ]
*/
namespace cli {
    class EnsembleEntry {
        sys_seconds timestamp;
        unordered_map</* symbol */ string, unordered_map</* generator */ string, unordered_map</* filter */ string, Signal>>> entries;
    };
    int ensemble(int argc, char * argv[]);
}
#endif