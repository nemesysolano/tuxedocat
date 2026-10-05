## CDFs ##
## The Bracket-Implied Gaussian Wave Packet
$$u(x, 0) = ( \frac{1}{2π σ^2(t)} )^{1/4} e^ { - \frac{(x - μ(t))^2}{4σ^2(t)}}$$

$$F_K(X, t) = \frac{1}{2} \left[ \text{erf}\left( \frac{X - μ(t)}{σ(t)\sqrt{2}} \right) - \text{erf}\left( \frac{x_{\min}(t) - μ(t)}{σ(t)\sqrt{2}} \right) \right]$$

### Kernel Density
$$F_K(X, t) = \frac{1}{2N} \sum_{j=0}^{N-1} \left[ \text{erf}\left( \frac{X - x_{t-j}}{h\sqrt{2}} \right) - \text{erf}\left( \frac{x_{\min}(t) - x_{t-j}}{h\sqrt{2}} \right) \right]$$

### Encoding Momentum**

$$F_K(X, t) = \frac{1}{2N} \sum_{j=0}^{N-1} \left[ \text{erf}\left( \frac{X - (x_{j} + vt)}{H(t)\sqrt{2}} \right) - \text{erf}\left( \frac{x_{\min} - (x_{j} + vt)}{H(t)\sqrt{2}} \right) \right]$$

## Derivatives ##

### The Bracket-Implied Gaussian Wave Packet Derivatives
To calculate the spatial and temporal derivatives for **The Bracket-Implied Gaussian Wave Packet**, let us first write out the wave packet function from `FourierSignals_2.md`:

$$u(x, t) = \left( \frac{1}{2\pi σ(t)^2} \right)^{1/4} \exp\left( - \frac{(x - μ(t))^2}{4σ(t)^2} \right)$$

For notational clarity, let:

* Amplitude factor: $C(t) = (2\pi σ(t)^2)^{-1/4}$
* Exponent: $E(x, t) = -\frac{(x - μ(t))^2}{4σ(t)^2}$
* Thus, $u(x, t) = C(t) e^{E(x, t)}$

---

#### 1. First Spatial Derivative ($\frac{\delta u}{\delta x}$)

Since the amplitude factor $C(t)$ depends only on time $t$, it acts as a constant with respect to spatial coordinate $x$. Applying the chain rule to the exponential term:

$$\frac{\delta u}{\delta x} = C(t) \cdot e^{E(x, t)} \cdot \frac{\delta}{\delta x}\left[ -\frac{(x - μ(t))^2}{4σ(t)^2} \right]$$

$$\frac{\delta u}{\delta x} = u(x, t) \left( -\frac{2(x - μ(t))}{4σ(t)^2} \right) = -\frac{x - μ(t)}{2σ(t)^2} \, u(x, t)$$

Substituting $u(x, t)$ back in:


$$\frac{\delta u}{\delta x} = -\frac{x - μ(t)}{2σ(t)^2} \left( \frac{1}{2\pi σ(t)^2} \right)^{1/4} \exp\left( - \frac{(x - μ(t))^2}{4σ(t)^2} \right)$$

---

#### 2. Second Spatial Derivative ($\frac{\delta^2 u}{\delta x^2}$)

Differentiating $\frac{\delta u}{\delta x}$ with respect to $x$ using the product rule:

$$\frac{\delta^2 u}{\delta x^2} = \frac{\delta}{\delta x} \left[ -\frac{x - μ(t)}{2σ(t)^2} \, u(x, t) \right]$$

$$\frac{\delta^2 u}{\delta x^2} = -\frac{1}{2σ(t)^2} \, u(x, t) - \frac{x - μ(t)}{2σ(t)^2} \left( -\frac{x - μ(t)}{2σ(t)^2} \, u(x, t) \right)$$

$$\frac{\delta^2 u}{\delta x^2} = \left[ \frac{(x - μ(t))^2 - 2σ(t)^2}{4σ(t)^4} \right] u(x, t)$$

---

#### 3. First Temporal Derivative ($\frac{\delta u}{\delta t}$)

Both the pre-factor $C(t)$, the equilibrium anchor $μ(t)$, and the volatility spread $σ(t)$ depend on time $t$. Using the product rule on $u(x, t) = C(t) e^{E(x, t)}$:

$$\frac{\delta u}{\delta t} = \frac{dC}{dt} e^{E} + C(t) e^{E} \frac{\partial E}{\partial t}$$

* **Pre-factor derivative:**

$$\frac{dC}{dt} = \frac{d}{dt} \left[ (2\pi)^{-1/4} σ(t)^{-1/2} \right] = -\frac{1}{2} (2\pi)^{-1/4} σ(t)^{-3/2} \frac{dσ}{dt} = -\frac{1}{2σ}\frac{dσ}{dt} C(t)$$


* **Exponent derivative w.r.t $t$:**

$$\frac{\partial E}{\partial t} = \frac{\partial}{\partial t} \left[ -\frac{(x - μ(t))^2}{4σ(t)^2} \right] = -\frac{2(x - μ(t))(-\frac{dμ}{dt})(4σ^2) - (x - μ(t))^2(8σ \frac{dσ}{dt})}{16σ^4}$$


$$\frac{\partial E}{\partial t} = \frac{(x - μ(t))\frac{dμ}{dt}}{2σ(t)^2} + \frac{(x - μ(t))^2 \frac{dσ}{dt}}{2σ(t)^3}$$



Combining them:


$$\frac{\delta u}{\delta t} = u(x, t) \left[ \frac{(x - μ(t))\frac{dμ}{dt} + \frac{(x - μ(t))^2}{σ(t)}\frac{dσ}{dt} - σ(t)\frac{dσ}{dt}}{2σ(t)^2} \right]$$

---

#### 4. Second Temporal Derivative ($\frac{\delta^2 u}{\delta t^2}$)

Differentiating $\frac{\delta u}{\delta t}$ with respect to $t$ requires applying the product and chain rules across the coupled temporal trajectories of $μ(t)$, $σ(t)$, $\frac{dμ}{dt}$, and $\frac{dσ}{dt}$.

In practical implementations within your C++ trading engine, computing the full analytical second temporal derivative analytically can be computationally heavy. Instead, because $u(x, t)$ is evaluated at discrete time steps $dt$, second temporal derivatives are typically discretized using finite differences:

$$\frac{\delta^2 u}{\delta t^2} \approx \frac{u(x, t) - 2u(x, t-dt) + u(x, t-2dt)}{dt^2}$$