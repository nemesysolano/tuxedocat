# augmented_dickey_fuller_test #

Let's create a simplified C++ version of `adfuller` function which is defined in `https://github.com/statsmodels/statsmodels/blob/main/statsmodels/tsa/tests/test_stattools.py`

```python
def adfuller(x, maxlag: int | None = None, regression="c", autolag="AIC", store=False, regresults=False)
```

The C++ version (namely **augmented_dickey_fuller_test**) is defined as

```C++
std::expected<AugmentedDickeyFuller, TuxedoError> augmented_dickey_fuller_test(
    slice::Span2D & x,
    RegressionType regression_type
)
```
