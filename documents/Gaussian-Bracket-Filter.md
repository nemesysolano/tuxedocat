# Gaussian Bracket Filter

Let $x_t > 0$ denote the price at discrete time $t$. For each $t$, define a
price bracket by the nearest observed prices on either side of $x_t$:

* $x_{\max}(t)$ is the nearest higher high, so $x_{\max}(t) > x_t$;
* $x_{\min}(t)$ is the nearest lower low, so $0 < x_{\min}(t) < x_t$.

The bracket is assumed to exist. At the beginning or end of a finite data set,
or when a price is an extreme, a caller must define a boundary policy (for
example, omit that observation or use a wider search range).

The current `ExtremePrice` strategy uses a carry-forward policy: when a
causal bracket does not exist, it emits the most recent valid $z(t)$ and an
idle signal. This can produce flat segments in the output during runs of new
highs or new lows; those segments are boundary-policy output, not additional
filter observations.

In the implementation, the bracket search is causal: $x_{\max}(t)$ is the
nearest prior bar high above the current bar high, and $x_{\min}(t)$ is the
nearest prior bar low below the current bar low. The prices used in $z(t)$
are the corresponding bar closes.

## Time-Dependent Variance

The time-dependent variance at $t$ is defined from the logarithmic width of the
bracket:

$$
σ^2(t) =
\frac{\left[\ln\left(x_{\max}(t) / x_{\min}(t)\right)\right]^2}
	{4\ln 2}.
$$

Using the log ratio makes the measure scale-invariant: multiplying all prices
by the same positive constant does not change $σ^2(t)$. The factor
$4\ln 2$ is a calibration constant inherited from the Gaussian-bracket
construction; it does not change the ordering of the variances.

The bracket must have nonzero width. If
$x_{\max}(t) = x_{\min}(t)$, then $σ^2(t) = 0$ and the inverse-variance
weight below is undefined.

## Inverse-Variance Weight

Define the raw inverse-variance weight by

$$
w(t) = \frac{1}{σ^2(t)}.
$$

Let $N = \min\{I(x_{\min}(t)), I(x_{\max}(t))\}$, where $I(x_{\min}(t))$ and $I(x_{\max}(t))$ are the numbers of bars from the current bar at time $t$ back to the nearest lower low and nearest higher high, respectively. For the causal window $[t-(N-1), t]$, define the raw inverse-variance weights locally for each lagged bar by

$$
 w(t-j) = \frac{1}{\sigma^2(t-j)}
 = \frac{4\ln 2}{\left[\ln\left(x_{\max}(t-j) / x_{\min}(t-j)\right)\right]^2},
\qquad j = 0, 1, \ldots, N-1.
$$

Then the normalized inverse-variance weight at lag $i$ is

$$
\hat w(t-i) = \frac{w(t-i)}{\sum_{j=0}^{N-1} w(t-j)}
$$

for $i = 0, 1, \ldots, N-1$. This guarantees

$$
\sum_{i=0}^{N-1} \hat w(t-i) = 1,
$$

provided all raw weights in the window are finite and positive. The raw weight $w(t)$ is not constrained to lie in $[0,1]$; only the normalized weight $\hat w(t)$ is.

The implementation omits a lagged bar when its bracket, variance, or close is
invalid, then renormalizes the remaining valid weights. If no valid weights
remain, $z(t)$ is invalid and the strategy applies the carry-forward policy
described above.

## Bracketed Moving Average $z(t)$

$$
z(t) = \sum^{N-1}_{i=0} x(t-i) \hat w(t-i)
$$ 