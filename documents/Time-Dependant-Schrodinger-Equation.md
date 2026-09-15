# Time-Dependent Schrödinger Equation for Risk Analysis #
This document outlines a framework for filtering trading signals—such as moving-average crossovers and oscillators—using probability distributions derived from the time-dependent Schrödinger equation and related quantum-inspired analytical methods.

In the first part, we describe the time-dependent Schrödinger equation and its classical solutions—namely the infinite well, the harmonic oscillator, and the free particle. Once these foundations are established, we introduce the technical indicators whose signals are rejected or discarded according to the risk identified by the probability distributions derived from the time-dependent Schrödinger framework.

## Time-Dependent Schrödinger Equation ##

$iℏ \frac{∂Ψ(x,t)}{∂t}=-\frac{ℏ^2}{2m}\frac{∂^2Ψ(x,t)}{∂x^2}+V(x,t)Ψ(x,t)$

The physical meaning of the terms and factors in each component can be found in standard physics textbooks; here, we focus on their financial interpretation.

### Terms and Factors ###

#### Dynamic Price Trend #### 

$iℏ \frac{∂Ψ(x,t)}{∂t}$

n Time-Dependant-Schrodinger-Equation.md, this term represents the time evolution of the system, acting as the quantum mechanical operator for total energy. In a financial model, it captures the dynamic price trend by calculating how the asset's probability wave function evolves over time. It determines how the likelihood of the asset reaching a specific price state shifts from the present into the future.

#### Market Volatility/Liquidity #### 

$-\frac{ℏ^2}{2m}\frac{∂^2Ψ(x,t)}{∂x^2}$

This spatial derivative component represents the kinetic energy of a particle. In quantum finance, the "mass" (m) of an asset is defined as the inverse of its volatility. The second derivative with respect to position (representing the asset's price) models price diffusion and dispersion. A highly volatile asset acts as a low-mass particle with rapid, broad probability distribution changes, whereas a highly liquid, stable asset behaves like a heavy particle with slower price acceleration.

Since the model is daily by construction, the effective mass is defined using the daily Parkinson volatility proxy. In this setting,

$m = \frac{1}{σ^2} = \frac{4 \ln 2}{\left(\ln \frac{x_\text{max}}{x_\text{min}}\right)^2}$.

where $x_\text{max}$ and $x_\text{min}$ are the nearest higher high and lower low, respectively. The importance of $σ$ is difficult to overstate, since it can also be used to construct dynamic filters whose lookback windows adapt to prevailing volatility. 

#### Market Sentiment/Fundamentals ####

$V(x,t)Ψ(x,t)$

This factor introduces the potential energy field acting upon the system. Financially, V(x,t) quantifies external market forces, macroeconomic fundamentals, and overarching sentiment that constrain or propel the asset's price. It acts as a structural boundary; high potential barriers represent strong market support and resistance levels, while a potential well illustrates a stock trading in equilibrium within a consolidated price range.

### The Classic Solutions ###

#### The Infinite Well

$V(x,t) = \begin{cases} 0 & \text{if } 0 < x < L \\ ∞ & \text{otherwise} \end{cases}$

The general solution for the infinite well case is:

$Ψ(x,t) = \sum_{n=1}^{∞} c_n \sqrt{\frac{2}{L}} \sin\left(\frac{nπ x}{L}\right) \exp\left(-i \frac{n^2 π^2 ℏ}{2m L^2} t\right)$

where the coefficients $c_n$ are determined by the initial price distribution state of the system $Ψ(x,0)$ at time $t=0$:

$c_n = \int_0^L Ψ(x,0) \sqrt{\frac{2}{L}} \sin\left(\frac{nπ x}{L}\right) dx$

In reality, prices range between $x_{\text{min}}$ and $x_{\text{max}}$. Therefore, to work with the infinite well we have to apply these transformations:

$x^* = x - x_{\text{min}}$

$L = x_{\text{max}} - x_{\text{min}}$.

The new formulae for the infinite well when using the two $x^*$ and $L$ transformations is

$Ψ(x,t) = \sum_{n=1}^{∞} c_n \sqrt{\frac{2}{L}} \sin\left(\frac{nπ(x-x_\text{min})}{L}\right) \exp\left(-i \frac{n^2 π^2 ℏ}{2m L^2} t\right)$ and

$c_n = \int_{x_\text{min}}^{x_\text{max}} Ψ(x,0) \sqrt{\frac{2}{L}} \sin\left(\frac{nπ(x-x_\text{min})}{L}\right) dx$.

The probability density for the infinite-well system is

$\vert{}Ψ(x,t)\vert{}^2 = \sum_{j=1}^{∞} \sum_{k=1}^{∞} c_j^* c_k \frac{2}{L} \sin\left(\frac{jπ(x-x_{\text{min}})}{L}\right) \sin\left(\frac{kπ(x-x_{\text{min}})}{L}\right) \exp\left(i \frac{(j^2 - k^2) π^2 ℏ}{2m L^2} t\right)$,

and the corresponding cumulative probability distribution is

$F(X,t) = \underbrace{\sum_{n=1}^{∞} c_n^2 \left( \frac{X - x_{\text{min}}}{L} - \frac{1}{2nπ} \sin\left(\frac{2nπ (X - x_{\text{min}})}{L}\right) \right)}_{\text{Static cumulative probability}} + \underbrace{\sum_{j=1}^{∞} \sum_{k>j}^{∞} \frac{2 c_j c_k}{π} \cos\left(\frac{(j^2 - k^2) π^2 ℏ}{2m L^2} t\right) \left[ \frac{\sin\left(\frac{(j-k)π (X - x_{\text{min}})}{L}\right)}{j-k} - \frac{\sin\left(\frac{(j+k)π (X - x_{\text{min}})}{L}\right)}{j+k} \right]}_{\text{Time-evolution cumulative probability}}$

As our engine processes an unnormalized raw initial state Ψ(x,0) where the resulting coefficients do not sum to 1, we need to introduce a normalization constant $A$ to
forces the system back into a valid 100% probability space. This constant is defined as

$A=\frac{1}{\sqrt{\sum^∞_{n=1}c^2_n}}$ and this constant makes this equation hold: $\int^{x_{\text{max}}}_{x_{\text{min}}}|AΨ(x,t)|^2dx = 1$.

$F_A(X,t) = A^2(\underbrace{\sum_{n=1}^{∞} c_n^2 \left( \frac{X - x_{\text{min}}}{L} - \frac{1}{2nπ} \sin\left(\frac{2nπ (X - x_{\text{min}})}{L}\right) \right)}_{\text{Static cumulative probability}} + \underbrace{\sum_{j=1}^{∞} \sum_{k>j}^{∞} \frac{2 c_j c_k}{π} \cos\left(\frac{(j^2 - k^2) π^2 ℏ}{2m L^2} t\right) \left[ \frac{\sin\left(\frac{(j-k)π (X - x_{\text{min}})}{L}\right)}{j-k} - \frac{\sin\left(\frac{(j+k)π (X - x_{\text{min}})}{L}\right)}{j+k} \right]}_{\text{Time-evolution cumulative probability}})$

----

If an oscillator generates a buy signal at price $X$, you evaluate $F_A(X,t)$. If $F_A(X,t)=0.95$, there is a 95% mathematical probability that the asset will trade **below** this level, indicating the asset is near the structural ceiling (resistance) and the **long signal should be heavily discounted or discarded**.

Conversely, if an oscillator generates a sell signal at price $X$ and the evaluation yields $1−F_A(X,t)=0.95$, there is a 95% mathematical probability that the asset will trade **above** that level. This indicates the asset is near a bouncing floor (support) where further downward movement is highly restricted, meaning the **short signal must be heavily discounted or discarded**.

#### The Harmonic Oscillator (Mean-Reverting Price Model)

For a harmonic oscillator where the potential is centered around an equilibrium or mean price $μ$, the potential function is $V(x) = \frac{1}{2}mω^2 (x - μ)^2$. The system is mathematically identical to the standard quantum harmonic oscillator, but with a simple coordinate shift.

The general solution for the time-dependent wave function is a linear superposition of the stationary states:

$Ψ(x,t) = \sum_{n=0}^{∞} c_n ψ_n(x) \exp\left(-i ω \left(n + \frac{1}{2}\right) t\right)$

The normalized spatial eigenfunctions $ψ_n(x)$ incorporate the shift $(x - μ)$ and are defined as:

$ψ_n(x) = \frac{1}{\sqrt{2^n n!}} \left(\frac{mω}{πℏ}\right)^{1/4} \exp\left(-\frac{mω (x - μ)^2}{2ℏ}\right) H_n\left(\sqrt{\frac{mω}{ℏ}}(x - μ)\right)$

* $n$ represents the discrete quantum state ($n = 0, 1, 2, \dots$).
* $H_n$ denotes the Hermite polynomials (e.g., $H_0(z)=1$, $H_1(z)=2z$, $H_2(z)=4z^2-2$).
* The exponential time-evolution term relies on the energy eigenvalues for these states, which remain $E_n = ℏω(n + \frac{1}{2})$.

The coefficients $c_n$ are determined by projecting the system's initial probability wave function at time zero, $Ψ(x,0)$, onto each eigenfunction:

$c_n = \int_{-∞}^{∞} Ψ(x,0) ψ_n(x) dx$

While standard quantum mechanics integrates from $-∞$ to $∞$, applying this to a financial framework often requires setting a lower boundary (since asset prices cannot drop below zero) or applying logarithmic transformations to the price inputs to preserve the validity of the unbounded integral.

Again, if we solve $\int^{x_{\text{max}}}_{x_{\text{min}}}|AΨ(x,t)|^2dx = 1$ then we get $A = \frac{1}{\sqrt{\int_{x_{\text{min}}}^{x_{\text{max}}} \vert{}\Psi(x,t)\vert{}^2 dx}}$.
Applying the scaling factor $A^2$ to the cumulative integral from the lower boundary $x_{\text{min}}$ to the target price $X$, with the spatial constant $α = \sqrt{\frac{mω}{\hbar}}$, yields the normalized cumulative density function:

$F_A(X,t) = A^2 \frac{α}{\sqrt{π}} \left[ \underbrace{\sum_{n=0}^{∞} \frac{c_n^2}{2^n n!} \int_{x_{\text{min}}}^X e^{-α^2(x-μ)^2} H_n^2(α(x-μ)) dx}_{\text{Static Cumulative Probability}} + \underbrace{\sum_{j=0}^{∞} \sum_{k>j}^{∞} \frac{2 c_j c_k \cos(ω(j-k)t)}{\sqrt{2^{j+k} j! k!}} \int_{x_{\text{min}}}^X e^{-α^2(x-μ)^2} H_j(α(x-μ)) H_k(α(x-μ)) dx}_{\text{Time-Evolution Cumulative Probability}} \right]$, where

$α = \sqrt{\frac{mω}{ℏ}}$

#### Free Particle

Because a pure, single-momentum free particle cannot be localized to a specific price (its probability distribution would be perfectly flat from $-\infty$ to $\infty$), the system must be modeled using a **Gaussian wave packet**. This represents an asset whose price is initially known with a high degree of certainty (localized around a starting price $x_0$) but possesses some baseline uncertainty $σ_0$.

If the asset has an initial price $x_0$ and an underlying price drift/momentum $p_0$, the time-evolved probability density function $\vert{}Ψ(x,t)\vert{}^2$ evaluates to a normal distribution whose variance strictly expands over time:

$\vert{}Ψ(x,t)\vert{}^2 = \frac{1}{\sqrt{2\pi σ(t)^2}} \exp\left( -\frac{(x - x_0 - vt)^2}{2σ(t)^2} \right)$

**Key System Components:**

* **Drift Velocity ($v$):** $v = p_0 / m$. This represents the asset's directional trend. If the asset has upward momentum, the center of the probability distribution drifts higher over time $t$.
* **Time-Dependent Variance ($σ(t)^2$):** The price dispersion spreads out as time moves forward, dictated by the equation:

$σ(t) = σ_0 \sqrt{1 + \left(\frac{ℏ t}{2mσ_0^2}\right)^2}$

