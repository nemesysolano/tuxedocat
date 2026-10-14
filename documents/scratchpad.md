Define $u_g(x,t)$ and $u_H(r,t)$  as

$u_g(x,t) = \frac{1}{σ(t) \sqrt{2π}} e^{ - \frac{(x - μ(t))^2}{2σ(t)^2} }$ and 

$u_h(r,t) = c \cdot \left( σ(t)^2 + (x - μ(t))^2 \right)^{\frac{λ- 1/2}{2}} K_{λ- 1/2}\left( α \sqrt{σ(t)^2 + (x - μ(t))^2} \right) e^{β(x - μ(t))}$

respectively. Now let's build the 

$$A \frac{δ^2 u_g}{δ t^2} + B \frac{δ u_g}{δ t} + C \frac{δ^2 u_g}{δ x^2} + D \frac{δ u_g}{δ x} + E \left( \frac{δ u_g}{δ x} \right)^2 + G \left( u \frac{δ u_g}{δ x} \right) + K u_g^3 + P u_g^2 + M \frac{\frac{δ u_g}{δ x}}{u_g} + Q \frac{\frac{δ^2 u_g}{δ x^2}}{u_g} + L \left( \log(u^2) \cos(u_g) \right) + S \left( \Vert{}u\Vert{} (\sin(u_g))^2 \right) = u_h(r,t)  $$

Furthermore,

Substituting your specific dynamic wave packet into the integral yields the formal CDF equation:

$$U_h(X ≤ x, t) = \int_{-\infty}^{x} c(t) \cdot \left( σ(t)^2 + (ξ - μ(t))^2 \right)^{\frac{λ - 1/2}{2}} K_{λ - 1/2}\left( α \sqrt{σ(t)^2 + (ξ - μ(t))^2} \right) e^{β(ξ - μ(t))} dξ$$

where $ξ$ is the dummy variable of integration representing the continuous spatial price grid, and the time-dependent normalizing constant $c(t)$ is defined to ensure the total area equals exactly $1.0$:

$$c(t) = \frac{\left(α^2 - β^2\right)^{λ/2}}{\sqrt{2π} α^{λ - 1/2} σ(t)^λ K_λ\left(σ(t) \sqrt{α^2 - β^2}\right)}$$


--
$$A \frac{δ^2 u_p}{δ t^2} + B \frac{δ u_p}{δ t} + C \frac{δ^2 u_p}{δ x^2} + D \frac{δ u_p}{δ x} + E \left( \frac{δ u_p}{δ x} \right)^2 + G \left( u \frac{δ u_p}{δ x} \right) + K u_p^3 + P u_p^2 + M \frac{\frac{δ u_p}{δ x}}{u_p} + Q \frac{\frac{δ^2 u_p}{δ x^2}}{u_p} + L \left( \log(u^2) \cos(u_p) \right) + S \left( \Vert{}u\Vert{} (\sin(u_p))^2 \right) = u_G(r,t)  $$

This non-linear PDE architecture acts as a dynamic transfer function, mapping the partial derivatives of a simpler, constrained baseline state ($u_p$) into an empirically accurate, complex market state ($u_G$). When the Eigen solver finds a high-$R^2$ fit for the coefficients, it mathematically proves that the current market micro-structure is smoothly warping the baseline probability into the target distribution.

Here are four advanced $(u_p, u_G)$ setups designed specifically to extract edge for One-Touch barrier and GTC Limit order strategies:

### 1. The Merton Jump-Diffusion Setup (Shock & Gap Modeling)

Standard models assume prices move in a continuous diffusion process. In reality, overnight gaps and sudden algorithmic shocks break continuity. This setup isolates the probability of discontinuous jumps.

* **The Baseline ($u_p$):** Standard Gaussian diffusion representing the continuous, "normal" market trading environment without shocks.

$$u_p(x,t) = \frac{1}{\sigma \sqrt{2\pi}} e^{ - \frac{(x - \mu)^2}{2\sigma^2} }$$


* **The Target ($u_G$):** A Poisson-driven Jump-Diffusion probability density. It models the return as a mixture of continuous Brownian motion and discrete jumps occurring at rate $\lambda$.

$$u_G(r,t) = \sum_{k=0}^{\infty} \frac{e^{-\lambda t} (\lambda t)^k}{k!} \cdot \frac{1}{\sqrt{2\pi (\sigma^2 t + k\delta^2)}} \exp\left( - \frac{(r - (\mu t + k\gamma))^2}{2(\sigma^2 t + k\delta^2)} \right)$$


* **Strategic Edge for GTC Limits:** Your current strategy loses trade density by discarding signals when the market gaps past the `--limit-offset`. By solving for the transfer coefficients mapping $u_p$ to a Jump-Diffusion $u_G$, the system can actively price the probability of an overnight gap. If the PDE flags a high probability of a jump ($\lambda > \text{threshold}$), the strategy can dynamically widen the `--limit-offset` to execute the trade rather than strictly skipping it.

### 2. The Student's t-Distribution Setup (Dynamic Tail Expansion)

While the Generalized Hyperbolic (GH) distribution is the master equation for skew and kurtosis, a localized Student's t-distribution setup is highly efficient for modeling pure fat tails without the computational overhead of Bessel functions.

* **The Target ($u_G$):** The Student's t-distribution parameterized by degrees of freedom $\nu(t)$, which scales the fatness of the tails.

$$u_G(r,t) = \frac{\Gamma(\frac{\nu(t)+1}{2})}{\sigma(t) \sqrt{\nu(t) \pi} \Gamma(\frac{\nu(t)}{2})} \left( 1 + \frac{(r - \mu(t))^2}{\nu(t) \sigma(t)^2} \right)^{-\frac{\nu(t)+1}{2}}$$


* **The Baseline ($u_p$):** The standard Gaussian distribution, which is the exact mathematical limit of the Student's t-distribution as $\nu \to \infty$.
* **Strategic Edge for One-Touch Options:** As market uncertainty increases, $\nu(t)$ decreases, shifting probability mass from the center into the extremes. Tracking the PDE coefficients mapping the $\nu \to \infty$ baseline into a low-$\nu$ target precisely identifies "tail expansion" regimes. You can programmatic scale the `--barrier-mult` dynamically based on the current $\nu(t)$ estimation: tightening targets to `0.25` during high-$\nu$ (Gaussian/mean-reverting) environments, and expanding targets to `1.0` during low-$\nu$ (fat-tail/breakout) environments.

### 3. Extreme Value Theory (EVT) Setup (Threshold Exceedance)

For barrier options, the shape of the probability distribution near the mean ($\mu$) is practically irrelevant. The only mathematical event that matters is whether the price exceeds the $x_{max}$ or $x_{min}$ threshold. EVT abandons the center mass entirely to model only the extremes.

* **The Target ($u_G$):** The Generalized Pareto Distribution (GPD), which models the probability density of returns strictly *given that they have already exceeded a high threshold* $u$.

$$u_G(r,t) = \frac{1}{\beta} \left( 1 + \xi \frac{r - u}{\beta} \right)^{-\left(\frac{1}{\xi} + 1\right)}$$


* **The Baseline ($u_p$):** The Exponential distribution, which is the proper subcase of the GPD when the shape parameter $\xi = 0$ (implying a light-tailed exponential decay of extreme returns).

$$u_p(x,t) = \frac{1}{\beta} e^{-\frac{x - u}{\beta}}$$


* **Strategic Edge for Barrier Multipliers:** If the PDE cleanly solves the mapping from an Exponential tail to a GPD tail with $\xi > 0$, it confirms the asset is currently in a state where structural breakouts have infinite momentum (heavy Pareto tails). This provides mathematical clearance to confidently hold positions to the full `1.0` barrier multiplier.

### 4. The Variance Gamma Setup (Business Time vs. Calendar Time)

Financial markets do not move in uniform clock time; they move in "business time" driven by trade volume and tick activity.

* **The Baseline ($u_p$):** A Gaussian distribution evaluating price over standard, uniform calendar time $t$.
* **The Target ($u_G$):** The Variance Gamma (VG) probability density function. It evaluates price as a continuous Brownian motion evaluated on a Gamma process (a randomized time clock).

$$u_G(r,t) = \int_{0}^{\infty} \frac{1}{\sigma \sqrt{2\pi g}} e^{-\frac{(r - \mu - \theta g)^2}{2\sigma^2 g}} \cdot \frac{g^{\frac{t}{\nu} - 1} e^{-\frac{g}{\nu}}}{\nu^{\frac{t}{\nu}} \Gamma(\frac{t}{\nu})} dg$$


* **Strategic Edge for Time Horizons:** Your strategy uses a fixed forward bar count (`trade_horizon = int(window_size)`). If market activity compresses (low volume), calendar time passes without generating structural volatility, causing options to expire worthless. A VG setup maps standard calendar-time diffusion into volume-adjusted time. If the PDE detects severe time deformation, the engine can reject the signal entirely, knowing the required volatility will not materialize before the fixed calendar-bar horizon expires.
