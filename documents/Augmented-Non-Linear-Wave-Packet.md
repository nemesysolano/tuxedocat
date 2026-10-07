# Augmented Non-Linear Wave Packet #

$$A \frac{δ^2 u}{δ t^2} + B \frac{δ u}{δ t} + C \frac{δ^2 u}{δ x^2} + D \frac{δ u}{δ x} + E \left( \frac{δ u}{δ x} \right)^2 + G \left( u \frac{δ u}{δ x} \right) + K u^3 + P u^2 + M \frac{\frac{δ u}{δ x}}{u} + Q \frac{\frac{δ^2 u}{δ x^2}}{u} + L \left( \ln(u^2) \cos(u) \right) + S \left( \Vert{}u\Vert{} (\sin(u))^2 \right) = f(x(t),t)  $$

## Financial Parameters of the Augmented Non-Linear Model

| Parameter | Coefficient Term | Financial / Physical Meaning | Stability Constraint |
| --- | --- | --- | --- |
| **$A$** | $\frac{\partial^2 u}{\partial t^2}$ | **Inertia / Acceleration:** The resistance of the market to sudden changes in trend velocity. | None (Implicitly handled) |
| **$B$** | $\frac{\partial u}{\partial t}$ | **Temporal Damping:** The natural decay of momentum and speculative excess over time. | **$B  ≤ 0$** (Must be positive to prevent runaway feedback loops) |
| **$C$** | $\frac{\partial^2 u}{\partial x^2}$ | **Spatial Diffusion:** The dispersion of price volatility across the local order book. | **$C > 0$** (Must be positive for stable forward diffusion) |
| **$D$** | $\frac{\partial u}{\partial x}$ | **Convective Drift:** The baseline directional trend or velocity of the asset. | None (Implicitly handled) |
| **$E$** | $\left( \frac{\partial u}{\partial x} \right)^2$ | **Kinetic Friction:** Non-linear drag that quadratically penalizes extreme price velocity spikes. | **$E  ≤ 0$** (Must act as a drag force) |
| **$G$** | $u \frac{\partial u}{\partial x}$ | **Shockwave (Burgers' Term):** Models asymmetric momentum flows, like sudden liquidity vacuums. | None |
| **$K$** | $u^3$ | **Self-Focusing (Herding):** The tendency for high volume (density) to attract more volume, pulling price toward equilibrium. | **$K > 0$** (Must be an attractive force) |
| **$P$** | $u^2$ | **Probability Gravity:** The structural potential well that anchors the asset to its mean $μ(t)$. | **$P  ≤ 0$** (Must form a deep restorative well) |
| **$M, Q$** | $\frac{u_x}{u}, \frac{u_{xx}}{u}$ | **Rational Phase Velocity:** Preserves geometric wave structure in the low-probability tails. | None (Requires epsilon floor in C++) |
| **$L$** | $\ln(u^2) \cos(u)$ | **Coupled Entropy/Lattice:** A decaying periodic well combining Shannon entropy with discrete price-grid support. | None |
| **$S$** | $\Vert{}u\Vert{} \sin^2(u)$ | **Absolute Lattice Friction:** Positive-definite "speed bumps" modeling localized liquidity clustering at discrete intervals. | None |

## External Market Forcing Field $f(x,t)$

$F(x,t)$ is the **external market forcing field** (the source term) in this inhomogeneous wave equation. It represents influences on the probability amplitude $u(x,t)$ that are not already explained by the model's inertia, damping, diffusion, drift, and nonlinear interactions. In the financial interpretation, these may include exogenous news, order-flow imbalances, or liquidity shocks, mapped onto price $x$ and time $t$. In practice

$$f(x(t),t) = \frac{x(t)-μ(t)}{σ(t)}$$

## The Probability Wave $u(x,t)$

The value of $u$ at any specific $(x,t)$ coordinate represents the exact mathematical likelihood that the asset's true consensus value is sitting at that specific price at that exact momen $t$.

Ultimately, $u(x,t)$ is the *space* one must measure to generate a trade. By running a numerical integral (calculating the area under the curve of u) over a specific target price range, your engine determines if the accumulated mass exceeds your 0.95 threshold, giving you the mathematical green light to execute.

Here are the three best theoretical options for $u(x,t)$, ranging from baseline execution to advanced fat-tailed physics:

To make the equation computationally viable in your C++ engine, we need an *Ansatz*—a trial functional form for the probability wave $u(x,t)$. This function must accurately represent market density, be twice-differentiable for your OLS feature matrix, and act as a valid probability density function (integrating to 1).

Here are the three best theoretical options for $u(x,t)$, ranging from baseline execution to advanced fat-tailed physics:

### 1. The Dynamic Gaussian Envelope (The Baseline)

The simplest and most computationally efficient shape is a standard normal distribution with time-varying parameters.

$$u(x,t) = \frac{1}{σ(t) \sqrt{2\pi}} e^{ - \frac{(x - μ(t))^2}{2σ(t)^2} }$$

* **Why it fits:** It is infinitely differentiable, making the calculation of $u_x$ and $u_{xx}$ extremely fast and perfectly smooth for your OLS matrix. The natural logarithm term $\ln(u^2)$ in your equation elegantly reduces to a simple parabola when applied to a Gaussian.
* **The Trade-off:** It assumes symmetric, thin tails, which will under-represent extreme outlier events in the order book.

### 2. The Generalized Hyperbolic (GH) Wave Packet (The Fat-Tailed Master)

Given the non-linear, shock-prone nature of the market, the wave packet is better modeled as a Generalized Hyperbolic distribution. This form natively captures the skewness and heavy tails observed in live equity tickers.

$$u(x,t) = c \cdot \left( σ(t)^2 + (x - μ(t))^2 \right)^{\frac{λ- 1/2}{2}} K_{λ- 1/2}\left( α \sqrt{σ(t)^2 + (x - μ(t))^2} \right) e^{β(x - μ(t))}$$

The parameter mapping is:

* **$μ(t)$ (Location):** The rolling 28-bar simple moving average.
* **$σ(t)$ (Scale):** The rolling 28-bar standard deviation.
* **$c(t)$ (Normalization):** The dynamically calculated scalar to ensure $\int u(x,t) dx = 1$.
* **$λ, α, β$ (Shape):** These define the fixed tail heaviness and structural skew of the asset's specific order book, estimated historically.
