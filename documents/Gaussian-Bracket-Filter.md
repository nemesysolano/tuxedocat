# Gaussian Bracket Filter

Let $x_t > 0$ denote the price at discrete time $t$. For each $t$, define a
price bracket by the nearest observed prices on either side of $x_t$:

* $x_{\max}(t)$ is the nearest higher high, so $x_{\max}(t) > x_t$;
* $x_{\min}(t)$ is the nearest lower low, so $0 < x_{\min}(t) < x_t$.

The bracket is assumed to exist. At the beginning or end of a finite data set,
or when a price is an extreme, a caller must define a boundary policy (for
example, omit that observation or use a wider search range).

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

For a causal window of $k$ observations, where $k \geq 1$, normalize the raw
weight at time $t$ by the weights in the window $t-k+1,\ldots,t$:

$$
\hat w(t) = \frac{w(t)}{\displaystyle\sum_{i=0}^{k-1} w(t-i)}.
$$

When all raw weights are finite and positive, the normalized weights satisfy
$0 < \hat w(t) \leq 1$ and the weights in each window sum to one. Because
$w_t$ is inversely proportional to $σ^2(t)$, a narrower bracket produces a
larger raw weight.

Although the construction is called a *Gaussian* bracket filter, the equations
above define an inverse-variance normalization, not the usual Gaussian kernel
$\exp(-u^2 / 2)$. The name refers to the calibration of the bracket variance.

## Filtered Series

The Gaussian bracket filtered series is obtained by pointwise rescaling:

$$
\hat x(t) = x_t\hat w(t)
$$

The $\hat x(t)$ is called **scaled price**. This operation does not average neighboring prices. It scales each price by
its normalized inverse-variance weight; therefore $\hat x_t$ has the same
price units as $x_t$, while its magnitude also reflects the local bracket
width.

The word *bracket* refers to the interval bounded by
$x_{\min}(t)$ and $x_{\max}(t)$.

## Trading Signals from Gaussian Bracket Filter

### Extreme Price Signals

Let $h(t)$ and $l(t)$ be the **high** and **low** prices at time $t$. Define
the adaptive trailing mean of the filtered series as

$$
z(t) = \frac{1}{N}\sum_{i=0}^{N-1} \hat x(t-i),
$$

where

$$
N = \min\{I(x_{\min}(t)), I(x_{\max}(t))\}.
$$

Here, $I(x_{\min}(t))$ and $I(x_{\max}(t))$ are the distances in bars from
the current bar at time $t$ to the bars containing the nearest lower low and
nearest higher high, respectively; furthermore $z(t)$ is called **gaussian bracketed average**. 
Thus, $N$ is a positive integer chosen so that the averaging window does not 
extend beyond the closer bracket extreme. The definition requires both extremes 
to exist and $N \geq 1$; a boundary policy is needed when either extreme is unavailable 
or occurs on the current bar.

The comparisons $l(t) > z(t)$ and $h(t) < z(t)$ may be tested as candidate
bullish and bearish signals, respectively. They are hypotheses rather than
guaranteed interpretations: each condition should be validated against a
subsequent return and tested out of sample.

