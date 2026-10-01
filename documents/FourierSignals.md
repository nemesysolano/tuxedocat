# Fourier Signals #

## General Second-Order Linear Inhomogeneus Partial Differential Equation

To solve the general second-order linear inhomogeneous partial differential equation

$$A \frac{δ^2u}{δ t^2} + B \frac{δ u}{δ t} + C \frac{δ^2u}{δ x^2} + D \frac{δ u}{δ x} = F(x, t)$$

using the Fourier transform, we transform the spatial domain ($x$) into the frequency domain ($ξ$). This reduces the PDE into a second-order Ordinary Differential Equation (ODE) in time ($t$).

### Step 1: Apply the Spatial Fourier Transform

Define the Fourier transform of $u(x, t)$ with respect to $x$ as:


$$\hat{u}(ξ, t) = \int_{-\infty}^{\infty} u(x, t) e^{-i ξ x} dx$$

Using the derivative properties of the Fourier transform ($\mathcal{F}\{\frac{δ^n u}{δ x^n}\} = (iξ)^n \hat{u}$), transform each spatial term:

* $\mathcal{F}\left\{\frac{δ^2 u}{δ x^2}\right\} = (iξ)^2 \hat{u} = -ξ^2 \hat{u}$
* $\mathcal{F}\left\{\frac{δ u}{δ x}\right\} = iξ \hat{u}$

Since the transform is with respect to $x$, time derivatives pass through unchanged:

* $\mathcal{F}\left\{\frac{δ u}{δ t}\right\} = \frac{d\hat{u}}{dt}$
* $\mathcal{F}\left\{\frac{δ^2 u}{δ t^2}\right\} = \frac{d^2\hat{u}}{dt^2}$

### Step 2: Formulate the Transformed ODE

Substitute the transformed terms back into the original PDE. Let $\hat{F}(ξ, t)$ be the Fourier transform of the source term $F(x, t)$:


$$A \frac{d^2\hat{u}}{dt^2} + B \frac{d\hat{u}}{dt} - C ξ^2 \hat{u} + i D ξ \hat{u} = \hat{F}(ξ, t)$$

Group the $\hat{u}$ terms to reveal a standard second-order linear ODE with constant coefficients (with respect to $t$, treating $ξ$ as a parameter):


$$A \frac{d^2\hat{u}}{dt^2} + B \frac{d\hat{u}}{dt} + (i D ξ - C ξ^2) \hat{u} = \hat{F}(ξ, t)$$

### Step 3: Solve the ODE in the Frequency Domain

#### Case 1: $A \neq 0$ (Hyperbolic/Wave-like Systems)
Solve the homogeneous equation by finding the roots of the characteristic polynomial $A r^2 + B r + (i D ξ - C ξ^2) = 0$:

$$r_{1,2}(ξ) = \frac{-B \pm \sqrt{B^2 - 4A(i D ξ - C ξ^2)}}{2A}$$

The general solution in the frequency domain is the sum of the homogeneous solution ($\hat{u}_h$) and a particular solution ($\hat{u}_p$) driven by $\hat{F}$:

$$\hat{u}(ξ, t) = c_1(ξ) e^{r_1(ξ) t} + c_2(ξ) e^{r_2(ξ) t} + \hat{u}_p(ξ, t)$$

Since $\hat{F}(ξ, t)$ can be any arbitrary function, the most general way to construct $\hat{u}_p(ξ, t)$ is using the **Method of Variation of Parameters**.

Given the two fundamental solutions from the homogeneous equation, $y_1(t) = e^{r_1(ξ) t}$ and $y_2(t) = e^{r_2(ξ) t}$, you compute the Wronskian:

$$W(y_1, y_2)(t) = y_1 y_2' - y_1' y_2 = (r_2 - r_1) e^{(r_1 + r_2)t}$$

The particular solution is then given by the integral formula:


$$\hat{u}_p(ξ, t) = -y_1(t) \int_0^t \frac{y_2(\tau) \frac{\hat{F}(ξ, \tau)}{A}}{W(\tau)} d\tau + y_2(t) \int_0^t \frac{y_1(\tau) \frac{\hat{F}(ξ, \tau)}{A}}{W(\tau)} d\tau$$

The constants $c_1(ξ)$ and $c_2(ξ)$ are found by transforming two initial conditions: $u(x, 0) = f(x) \implies \hat{u}(ξ, 0) = \hat{f}(ξ)$ and $\frac{δ u}{δ t}(x, 0) = g(x) \implies \frac{d\hat{u}}{dt}(ξ, 0) = \hat{g}(ξ)$.

#### Case 2: $A = 0$ (Parabolic/Diffusion/Drift Systems)**
If $A = 0$, the equation drops to a first-order ODE, typical of convection-diffusion equations common in quantitative finance and momentum modeling:


$$B \frac{d\hat{u}}{dt} + (i D ξ - C ξ^2) \hat{u} = \hat{F}(ξ, t)$$

This is solved using a standard integrating factor $I(t) = e^{\frac{i D ξ - C ξ^2}{B} t}$:


$$\hat{u}(ξ, t) = \hat{u}(ξ, 0) e^{-\left(\frac{i D ξ - C ξ^2}{B}\right)t} + \int_0^t \frac{\hat{F}(ξ, τ)}{B} e^{-\left(\frac{i D ξ - C ξ^2}{B}\right)(t-τ)} dτ$$

### Step 4: Apply the Inverse Fourier Transform

Once $\hat{u}(ξ, t)$ is explicitly constructed in the frequency domain, invert it back to the physical $(x, t)$ domain.


$$u(x, t) = \frac{1}{2π} \int_{-\infty}^{\infty} \hat{u}(ξ, t) e^{i ξ x} dξ$$

For simpler cases (like when $F=0$), this integral can often be resolved into a convolution between the initial conditions and a Gaussian kernel (the fundamental solution of the system).

## General Second-Order Linear Inhomogeneus Partial Differential Equation

Let $N$ be defined as in `Gaussian-Bracket-Filter.md`, $F(x,t) = \frac{x - μ(t)}{μ(t)}$. If we solve $\frac{1}{2π} \int_{-\infty}^{\infty} |A\hat{u}(ξ, t) e^{i ξ x}|^2 dξ=1$ integral equation for $A$ then
the resulting probability function is:

By defining the dimensionless price state as $y(x,t) = \frac{x - μ(t)}{μ(t)}$ and transforming from the Fourier (momentum) space back to the physical price domain, the resulting probability functions are mathematically expressed as follows.

## Distance Probability

Let $N$ defined as in `Gaussian-Bracket-Filter.md`, $F(x,t) = \frac{x - μ(t)}{μ(t)}$. If we solve $\frac{1}{2π} \int_{-\infty}^{\infty} |K\hat{u}(ξ, t) e^{i ξ x}|^2 dξ$ integral equation for $K$. Using Plancherel's theorem, the normalization constant $K$ that forces the system into a valid 100% probability space can be extracted directly from the frequency (momentum) domain $ξ$ without needing to integrate across physical prices:

$$K = \left( \frac{1}{2π} \int_{-\infty}^{\infty} \vert{}\hat{u}(ξ, t)\vert{}^2 dξ \right)^{-1/2}$$

The normalized wavefunction in the physical price domain is constructed by applying the inverse Fourier transform to the momentum-space wavefunction $K\hat{u}(ξ, t)$, substituting the scale-invariant fractional deviation $y(x,t)$:

$$Ψ(x, t) = \frac{K}{2π} \int_{-\infty}^{\infty} \hat{u}(ξ, t) e^{i ξ (\frac{x - μ(t)}{μ(t)})} dξ$$

The probability density function $ρ(x,t)$ (namely the **distance probability**) describes the exact likelihood of the asset trading at a specific nominal price $x$. Because the wavefunction was evaluated on the dimensionless variable $y = \frac{x - μ}{μ}$, you must apply the chain rule of probability measure ($dy = \frac{1}{μ(t)} dx$) to express the density relative to the absolute dollar price $x$:

$$ρ(x, t) = \frac{1}{μ(t)} \vert{}Ψ(x, t)\vert{}^2 = \frac{\vert{}K\vert{}^2}{μ(t)} \left\vert{} \frac{1}{2π} \int_{-\infty}^{\infty} \hat{u}(ξ, t) e^{i ξ (\frac{x - μ(t)}{μ(t)})} dξ \right\vert{}^2$$

The cumulative probability $F_K(X,t)$ is computed by integrating the PDF from the causal structural floor, $x_{\min}(t)$, up to the execution target price $X$:

$$F_K(X, t) = \int_{x_{\min}(t)}^{X} ρ(x, t) dx = \frac{\vert{}A\vert{}^2}{μ(t)} \int_{x_{\min}(t)}^{X} \left\vert{} \frac{1}{2π} \int_{-\infty}^{\infty} \hat{u}(ξ, t) \exp\left(i ξ \frac{x - μ(t)}{μ(t)}\right) dξ \right\vert{}^2 dx$$


$$F_K(X, t) = \frac{1}{2} \left[ \text{erf}\left( \frac{X - \mu(t)}{\sigma(t)\sqrt{2}} \right) - \text{erf}\left( \frac{x_{\min}(t) - \mu(t)}{\sigma(t)\sqrt{2}} \right) \right]$$

---

We abstracted the known past as μ(t) to illustrate the model's flexibility. This ensures that any valid equilibrium metric can be plugged into the state function F, making z(t) simply one instance of a broader baseline.

### Implementation in the Trading Engine

By evaluating $F_K(X, t)$, the engine maps momentum-space acceleration directly to price-level structural risk:

* **Long Filter:** If a buy signal fires at price $X$ and $F_K(X,t) \ge 0.95$, there is a 95% mathematical probability that the asset will trade below this level, and the long signal should be heavily discounted or discarded.


* **Short Filter:** If a sell signal fires at price $X$ and $1 - F_K(X,t) \ge 0.95$, there is a 95% probability the asset will trade above this level, meaning the short signal must be heavily discounted or discarded.

To construct the initial wavefunction $u(x,0)$ from a discrete price series, you must map observable market data to the two fundamental components of a quantum wave: **probability amplitude** (the spread of prices) and **phase** (the directional momentum).

Since the physical probability density is $\rho(x) = \vert{}u(x)\vert{}^2$, any candidate for the real part of $u(x,0)$ should essentially be the square root of an empirical or statistical price distribution.

Here are the most robust candidates for building $u(x,0)$ directly from your C++ price series data:
### Choosing $u(t)$

#### 1. The Bracket-Implied Gaussian Wave Packet

This is the most natural fit for your existing architecture. It assumes the asset's initial state is normally distributed around your equilibrium anchor, bounded by recent volatility.

* **Formulation:**

$$u(x, 0) = ( \frac{1}{2π σ^2(t)} )^{1/4} e^ { - \frac{(x - μ(t))^2}{4σ^2(t)}}$$


* **Data Mapping:**
  - Set $μ(t) = z(t)$ (the bracketed moving average).
  - Set $σ^2(t) = \frac{[\ln(x_{\max}(t) / x_{\min}(t))]^2}{4\ln 2}$ (your time-dependent bracket variance).
* **Why it works:** It perfectly seeds the Free Particle and Harmonic Oscillator solutions. It is mathematically smooth, guarantees $A=1$ normalization out of the gate, and instantly adapts to expanding/contracting volatility.
* **F_K(X, t)**:

$$F_K(X, t) = \frac{1}{2} \left[ \text{erf}\left( \frac{X - \mu(t)}{\sigma(t)\sqrt{2}} \right) - \text{erf}\left( \frac{x_{\min}(t) - \mu(t)}{\sigma(t)\sqrt{2}} \right) \right]$$

#### 2. The Empirical Volume Profile (Liquidity as Probability)

In market micro-structure, probability density clusters where trading volume clusters. You can define the initial wavefunction purely based on the historical volume transacted at each price level over your lookback window $N$.

* **Formulation:**

$$u(x, 0) = K \sqrt{\text{VP}(x)}$$


* **Data Mapping:**
* $\text{VP}(x)$ is the Volume Profile—a histogram of volume traded at price bin $x$ over the last $N$ bars.
* $K$ is the normalization constant required to ensure the integral of $\vert{}u(x,0)\vert{}^2$ equals 1.
* **Why it works:** This creates an irregular, realistic wave function. Support and resistance levels emerge naturally as high-density "peaks" in the wave, making it highly effective for modeling the Infinite Well's bounding walls.
* **F_K(X,t)**:
$$F_K(X, t) = \frac{\sum_{j=0}^{N-1} V_{t-j} \left( \frac{\max\big(0, \min(X, \text{High}_{t-j}) - \text{Low}_{t-j}\big)}{\text{High}_{t-j} - \text{Low}_{t-j}} \right)}{\sum_{j=0}^{N-1} V_{t-j}}$$
---
The formulation for $\text{VP}(x)$ is

$$\text{VP}(x) = \sum_{j=0}^{N-1} I_j(x) \left( \frac{V_{t-j}}{\text{High}_{t-j} - \text{Low}_{t-j}} \right)$$

where $I_j(x)$ is defined as

$$I_j(x) = \begin{cases}
1, & \text{if } \mathrm{Low}_{t-j} \le x \le \mathrm{High}_{t-j}, \\
0, & \text{otherwise}.
\end{cases}$$

#### 3. Kernel Density Estimation (KDE) of the $N$-Bar Window

If you want a continuous, mathematically differentiable function without assuming a strict Gaussian shape, you can build $u(x,0)$ by superimposing small probability kernels (like miniature Gaussians) on top of the closing prices of the last $N$ bars.

* **Formulation:**

$$u(x, 0) = K \sqrt{ \frac{1}{N} \sum_{j=0}^{N-1} e^{- \frac{(x - x_{t-j})^2}{2h^2}  }}$$


* **Data Mapping:**
* $x_{t-j}$ are the closing prices of the last $N$ bars.
* $h$ is the bandwidth (which can be set dynamically to a fraction of the ATR or the bracket width).
* **Why it works:** It smoothly bridges discrete price action and continuous PDEs. It creates a multi-modal wave function if the asset has been consolidating in two distinct ranges, natively capturing complex price geometries.
* **F_K(X,t)**:
$$F_K(X, t) = \frac{1}{2N} \sum_{j=0}^{N-1} \left[ \text{erf}\left( \frac{X - x_{t-j}}{h\sqrt{2}} \right) - \text{erf}\left( \frac{x_{\min}(t) - x_{t-j}}{h\sqrt{2}} \right) \right]$$

where

$$h=1.06⋅\hat σ⋅N^{−1/5}$$

#### 4. Encoding Momentum: The Complex Phase Factor

In quantum mechanics, a purely real wavefunction represents a particle at rest (no momentum). Because your engine relies heavily on price speed and acceleration ($\text{v}_{\text{current}}$, $\text{v}_{\text{previous}}$), you must inject this directional momentum into the wavefunction using a complex phase factor.

Whichever real amplitude candidate $R(x)$ you choose from above, multiply it by the momentum phase:


$$u(x, 0) = R(x) e^{i p_0 x}$$

* **Data Mapping for Momentum ($p_0$):** You can define the initial momentum $p_0$ directly from your engine's speed metric: $p_0 = \text{v}_{\text{current}} = \frac{\text{Close}_{t} - \text{Close}_{t-1}}{\text{Close}_{t-1}}$.
* **The Result:** If $p_0$ is positive, the complex phase encodes an upward drift. When you feed this $u(x,0)$ into the time-dependent Schrödinger equation, the probability wave will mathematically "flow" to the right (higher prices) over time, allowing your risk filters to evaluate moving targets.
* **F_K(X,T)**

$$F_K(X, t) = \frac{1}{2N} \sum_{j=0}^{N-1} \left[ \text{erf}\left( \frac{X - (x_{j} + vt)}{H(t)\sqrt{2}} \right) - \text{erf}\left( \frac{x_{\min} - (x_{j} + vt)}{H(t)\sqrt{2}} \right) \right]$$

where

$$H(t) = h \sqrt{1 + \left(\frac{\hbar t}{2}\right)^2}$$
$$h=1.06⋅\hat σ⋅N^{−1/5}$$

---
$R(x)$ represents the strictly real **probability amplitude** of the initial wavefunction before any directional momentum ($p_0$) is applied. Because the physical probability density is $\rho(x) = \vert{}R(x)\vert{}^2$, the formula for $R(x)$ is always the square root of your chosen initial probability distribution.

Based on the quantitative architecture established for your C++ engine, $R(x)$ takes one of three specific functional forms depending on the model you select:

**1. The Bracket-Implied Gaussian Wave Packet**

If you assume the asset's baseline price distribution is normally distributed around the deterministic anchor $\mu(t)$ (such as the bracketed moving average $z(t)$), $R(x)$ is the square root of a normal distribution with time-dependent variance $\sigma^2(t)$:

$$R(x) = \left( \frac{1}{2\pi \sigma^2(t)} \right)^{1/4} \exp\left( - \frac{(x - \mu(t))^2}{4\sigma^2(t)} \right)$$

**2. Kernel Density Estimation (KDE)**

If you build a multi-modal probability distribution from the individual closing prices $x_{t-j}$ of your $N$-bar lookback window, $R(x)$ is the normalized sum of miniature Gaussian wave packets (kernels) smoothed by bandwidth $h$:

$$R(x) = \frac{1}{\left(h^2 2\pi\right)^{1/4}} \sqrt{ \frac{1}{N} \sum_{j=0}^{N-1} \exp\left( - \frac{(x - x_{t-j})^2}{2h^2} \right) }$$

**3. The Empirical Volume Profile**

If you construct the wave function using physical trading liquidity to map structural support and resistance, $R(x)$ is the square root of the Volume Profile $\text{VP}(x)$ normalized by the total volume of the $N$-bar window:

$$R(x) = \frac{1}{\sqrt{\sum_{j=0}^{N-1} V_{t-j}}} \sqrt{\text{VP}(x)}$$

By substituting any of these three formulas into $u(x, 0) = R(x) e^{i p_0 x}$, you perfectly encode both the asset's structural risk (via $R(x)$) and its trending speed (via $p_0$) for the Schrödinger time-evolution calculations.



