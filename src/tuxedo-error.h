#ifndef __TUXEDO_ERROR_H__
#define __TUXEDO_ERROR_H__

typedef enum TuxedoErrorEnum {
    NO_ERROR = 0,
    ERR_ARR_INDEX_OUT_OF_BOUNDS = 1,
    ERR_EMPTY_VECTOR = 2,
    ERR_INVALID_REGRESSION_TYPE = 3,
    ERR_NO_OBSERVATIONS = 4
} TuxedoError;
#endif