# Augmented Non-Linear Wave Packet #

$$A \frac{δ^2 u}{δ t^2} + B \frac{δ u}{δ t} + C \frac{δ^2 u}{δ x^2} + D \frac{δ u}{δ x} + E \left( \frac{δ u}{δ x} \right)^2 + G \left( u \frac{δ u}{δ x} \right) + K u^3 + P u^2 + M \frac{\frac{δ u}{δ x}}{u} + Q \frac{\frac{δ^2 u}{δ x^2}}{u} + L \left( \log(u^2) \cos(u) \right) + S \left( \Vert{}u\Vert{} (\sin(u))^2 \right) = f(r,t)  $$

## Financial Parameters of the Augmented Non-Linear Model

The probability wave $u(r(t),t)$, where $r(t)=\log(\frac{x(t)}{x(t-1)})$ now measures the likelihood of the asset exhibiting a specific return velocity at a given moment.

| Parameter | Coefficient Term | Financial / Physical Meaning | Stability Constraint |
| --- | --- | --- | --- |
| **$A$** | $\frac{\partial^2 u}{\partial t^2}$ | **Volatility Inertia:** The resistance of the market to sudden changes in volatility acceleration (rate of change of momentum). | None (Implicitly handled) |
| **$B$** | $\frac{\partial u}{\partial t}$ | **Volatility Damping:** The natural decay of volatility over time, modeling the market "cooling off" after a violent session. | **$B \le 0$** (Must dampen runaway feedback loops) |
| **$C$** | $\frac{\partial^2 u}{\partial r^2}$ | **Return Variance Dispersion:** The dispersion of return magnitudes, directly mapping to the standard deviation of returns. | **$C > 0$** (Must be positive for stable forward diffusion) |
| **$D$** | $\frac{\partial u}{\partial r}$ | **Acceleration / Skewness Drift:** The baseline acceleration of the asset, physically modeling the inherent skew of the return distribution. | None (Implicitly handled) |
| **$E$** | $\left( \frac{\partial u}{\partial r} \right)^2$ | **Acceleration Friction:** Non-linear drag that quadratically penalizes extreme "fat-tail" outlier returns (flash crashes or violent squeezes). | **$E \le 0$** (Must act as a drag force) |
| **$G$** | $u \frac{\partial u}{\partial r}$ | **Volatility Clustering (Burgers' Term):** Models how a sudden, massive return (a shock) warps the distribution, causing leptokurtosis. | None |
| **$K$** | $u^3$ | **Volatility Resting State:** The tendency for returns to cluster heavily, absorbing volatility shocks and pulling velocity back to equilibrium. | **$K > 0$** (Must be an attractive force) |
| **$P$** | $u^2$ | **Return Gravity:** The structural potential well that anchors the expected return $r(t)$ back toward zero (or a baseline risk-free drift). | **$P \le 0$** (Must form a deep restorative well) |
| **$M, Q$** | $\frac{u_r}{u}, \frac{u_{rr}}{u}$ | **Rational Phase Velocity:** Preserves geometric wave structure in the extreme, low-probability return tails. | None (Requires epsilon floor in C++) |
| **$L$** | $\log(u^2) \cos(u)$ | **Coupled Entropy/Lattice:** A decaying periodic well combining Shannon entropy with the discrete constraints of minimum tick increments. | None |
| **$S$** | $\Vert{}u\Vert{} \sin^2(u)$ | **Tick-Size Friction:** Positive-definite "speed bumps" modeling the microscopic clustering of returns around specific fractional increments. | None |

## External Market Forcing Field $f(r,t)$

By calculating the distribution parameters (μ, σ, skewness, and kurtosis) strictly over the window $[t−N,t−1]$, and then evaluating the newly arrived return $r(t)=\log(x(t−1)/x(t))$ against those parameters, we 
transform the model from a descriptive smoothing function into a forward-looking predictive boundary.

$f(r(t),t) = \frac{r(t)-μ(t-1)}{σ{(t-1)}}$ 

## The Probability Wave $u(x,t)$

The value of $u$ at any specific $(r,t)$ coordinate represents the mathematical likelihood that the asset's true momentum is sitting at a specific level $r$ at that exact moment $t$.

Ultimately, $u(r,t)$ is the *space* one must measure to generate a trade. By running a numerical integral (calculating the area under the curve of u) over a specific target price range, your engine determines if the accumulated mass exceeds your 0.95 threshold, giving you the mathematical green light to execute.

Here are the three best theoretical options for $u(r,t)$, ranging from baseline execution to advanced fat-tailed physics:

To make the equation computationally viable in your C++ engine, we need an *Ansatz*—a trial functional form for the probability wave $u(r,t)$. This function must accurately represent market density, be twice-differentiable for your OLS feature matrix, and act as a valid probability density function (integrating to 1).

Here are the three best theoretical options for $u(r,t)$, ranging from baseline execution to advanced fat-tailed physics:

### 1. The Dynamic Gaussian Envelope (The Baseline)

The simplest and most computationally efficient shape is a standard normal distribution with time-varying parameters.

$$u(x,t) = \frac{1}{σ(t) \sqrt{2π}} e^{ - \frac{(x - μ(t))^2}{2σ(t)^2} }$$

* **Why it fits:** It is infinitely differentiable, making the calculation of $u_x$ and $u_{xx}$ extremely fast and perfectly smooth for your OLS matrix. The natural logarithm term $\log(u^2)$ in your equation elegantly reduces to a simple parabola when applied to a Gaussian.
* **The Trade-off:** It assumes symmetric, thin tails, which will under-represent extreme outlier events in the order book.

### 2. The Generalized Hyperbolic (GH) Wave Packet (The Fat-Tailed Master)

Given the non-linear, shock-prone nature of the market, the wave packet is better modeled as a Generalized Hyperbolic distribution. This form natively captures the skewness and heavy tails observed in live equity tickers.

$$u(r,t) = c \cdot \left( σ(t)^2 + (x - μ(t))^2 \right)^{\frac{λ- 1/2}{2}} K_{λ- 1/2}\left( α \sqrt{σ(t)^2 + (x - μ(t))^2} \right) e^{β(x - μ(t))}$$

The parameter mapping is:

* **$μ(t)$ (Location):** The rolling 28-bar simple moving average.
* **$σ(t)$ (Scale):** The rolling 28-bar standard deviation.
* **$c(t)$ (Normalization):** The dynamically calculated scalar to ensure $\int u(x,t) dx = 1$.
* **$λ, α, β$ (Shape):** These define the fixed tail heaviness and structural skew of the asset's specific order book, estimated historically.

----
Because exact Maximum Likelihood Estimation (MLE) requires iterative optimization algorithms (like Expectation-Maximization) that are far too slow for a tick-level C++ event loop, you must use the **Method of Moments (MoM)**. By calculating the rolling Skewness and Excess Kurtosis of your asset, you can map those standardized moments directly to $β$ and $α$.

To maintain execution speed, it is highly recommended to fix the kurtosis family index $λ$. Setting $λ = -0.5$ reduces the GH distribution to a **Normal Inverse Gaussian (NIG)** distribution, which possesses closed-form algebraic relationships between its parameters and its statistical moments, making it computationally trivial to solve on the fly.

Assuming $λ = -0.5$, the structural mapping from rolling moments to GH parameters is:

1. **Skewness ($S$):** Represents the asymmetry of the rolling window.

$$S = \frac{\sum^{N-1}_0 (r(t-i) - μ(t))^3}{N \cdot σ(t)^3}$$


2. **Excess Kurtosis ($K$):** Represents the fatness of the tails.

$$K = \frac{\sum^{N-1}_0 (r(t-i) - μ(t))^4}{N \cdot σ(t)^4} - 3$$


3. **Mapping $β$ (Skewness Parameter):**
$β$ is directly proportional to the observed skewness.

$$β \approx \frac{3 \cdot S}{σ(t)  \cdot \sqrt{K}}$$

4. **Mapping $α$ (Tail Heaviness):**
$α$ is inversely proportional to the observed kurtosis. As kurtosis spikes during a volatility shock, $α$ decreases, widening the tails of the density envelope.

$$α \approx \sqrt{β^2 + \frac{3}{K \cdot σ(t)^2}}$$

### The C++ Dual-Window Architecture

Estimating 3rd and 4th order moments (Skewness and Kurtosis) requires a significantly larger sample size than estimating the 1st and 2nd moments ($μ$ and $σ $). If you attempt to calculate kurtosis on your 28-bar window, a single outlier tick will cause the $x^4$ term to explode, sending $α$ into violent numerical oscillation and destabilizing your OLS matrix.

You must decouple the parameter estimation into a **Dual-Window Architecture**:

* **Fast Window (28 Bars):** Continuously updates $μ(t)$ and $σ(t)$. This ensures the wave packet stays perfectly centered on the current price action and scales with immediate localized volatility.
* **Slow Window (360+ Bars):** Continuously updates $S(t)$ and $K(t)$ to calculate $α$ and $β$. The structural shape of an asset's tail risk changes much slower than its absolute price.

### The Generalized Hyperbolic (GH) Wave Packet CDF

There is no closed-form analytical solution for the Cumulative Distribution Function (CDF) of the Generalized Hyperbolic (GH) distribution.

Because the probability density function (PDF) relies on the modified Bessel function of the second kind ($K$) entangled with a polynomial root and an exponential skew, its anti-derivative cannot be expressed using elementary algebraic terms or standard special functions (like the `std::erf` error function used for normal distributions).

The CDF, denoted as $P(X ≤ x, t)$, must be defined strictly as the definite integral of your probability fluid from negative infinity up to the target price $x$:

$$P(X ≤ x, t) = \int_{-\infty}^{x} u(ξ, t) dξ$$

Substituting your specific dynamic wave packet into the integral yields the formal CDF equation:

$$P(X ≤ x, t) = \int_{-\infty}^{x} c(t) \cdot \left( σ(t)^2 + (ξ - μ(t))^2 \right)^{\frac{λ - 1/2}{2}} K_{λ - 1/2}\left( α \sqrt{σ(t)^2 + (ξ - μ(t))^2} \right) e^{β(ξ - μ(t))} dξ$$

Where $ξ$ is the dummy variable of integration representing the continuous spatial price grid, and the time-dependent normalizing constant $c(t)$ is defined to ensure the total area equals exactly $1.0$:

$$c(t) = \frac{\left(α^2 - β^2\right)^{λ/2}}{\sqrt{2π} α^{λ - 1/2} σ(t)^λ K_λ\left(σ(t) \sqrt{α^2 - β^2}\right)}$$

### Engineering Implications for C++

Because this integral lacks a closed-form algebraic expression, your trading engine is mathematically barred from calculating exact GH tail probabilities in $O(1)$ time.

This mathematical reality dictates your C++ execution pipeline:

1. **No `std::erf` Equivalent:** You cannot use a single standard library lookup to map the Z-score to a probability percentage when using the GH *Ansatz*.
2. **Required Numerical Quadrature:** To calculate whether the current price has crossed the $p > 0.95$ or $p < 0.05$ execution boundary, the engine must actively compute the discrete area under the curve using numerical integration (such as the Simpson's Rule integrator, or a more advanced Gauss-Kronrod quadrature) over the fixed interval $[μ - 6σ , x]$.
3. **Latency Cost:** Evaluating the Bessel function $K$ at every discrete step of the integral loop introduces nanosecond-to-microsecond latency. This is the exact engineering tradeoff between the lightning-fast approximation of the Dynamic Gaussian model and the heavy, precise, fat-tail capture of the Generalized Hyperbolic model.

### C++ Implementation via Welford's Online Algorithm

To compute these higher-order moments in $O(1)$ time without iterating over the 360-bar array on every tick, you implement Welford's online algorithm for skewness and kurtosis within your state management.
