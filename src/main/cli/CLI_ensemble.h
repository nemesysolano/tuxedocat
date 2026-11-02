#ifndef __CLI_ENSEMBLED_H__
#define __CLI_ENSEMBLED_H__
#include "nanodbc/nanodbc.h"
#include <unordered_map>
#include <string>
#include "events/SignalEvent.h"
#include <vector>

using namespace std;
using namespace events;

/* 
    [
        {
            "timestamp": "2001-12-31T00:00:00.00"
            {
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
    
    int ensemble(int argc, char * argv[]);
}
#endif