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


----
$$A \frac{δ^2 u_p}{δ t^2} + B \frac{δ u_p}{δ t} + C \frac{δ^2 u_p}{δ x^2} + D \frac{δ u_p}{δ x} + E \left( \frac{δ u_p}{δ x} \right)^2 + G \left( u \frac{δ u_p}{δ x} \right) + K u_p^3 + P u_p^2 + M \frac{\frac{δ u_p}{δ x}}{u_p} + Q \frac{\frac{δ^2 u_p}{δ x^2}}{u_p} + L \left( \log(u^2) \cos(u_p) \right) + S \left( \Vert{}u\Vert{} (\sin(u_p))^2 \right) = u_G(r,t)  $$

This non-linear PDE architecture acts as a dynamic transfer function, mapping the partial derivatives of a simpler, constrained baseline state ($u_p$) into an empirically accurate, complex market state ($u_G$). When the Eigen solver finds a high-$R^2$ fit for the coefficients, it mathematically proves that the current market micro-structure is smoothly warping the baseline probability into the target distribution.

Here are four advanced $(u_p, u_G)$ setups designed specifically to extract edge for One-Touch barrier and GTC Limit order strategies:

### 1. The Merton Jump-Diffusion Setup (Shock & Gap Modeling)

Standard models assume prices move in a continuous diffusion process. In reality, overnight gaps and sudden algorithmic shocks break continuity. This setup isolates the probability of discontinuous jumps.

* **The Baseline ($u_p$):** Standard Gaussian diffusion representing the continuous, "normal" market trading environment without shocks.

$$u_p(x,t) = \frac{1}{σ \sqrt{2π}} e^{ - \frac{(x - μ)^2}{2σ^2} }$$


* **The Target ($u_G$):** A Poisson-driven Jump-Diffusion probability density. It models the return as a mixture of continuous Brownian motion and discrete jumps occurring at rate $λ$.

$$u_G(r,t) = \sum_{k=0}^{\infty} \frac{e^{-λ t} (λ t)^k}{k!} \cdot \frac{1}{\sqrt{2π (σ^2 t + kδ^2)}} \exp\left( - \frac{(r - (μ t + kγ))^2}{2(σ^2 t + kδ^2)} \right)$$

* **Strategic Edge for GTC Limits:** By solving for the transfer coefficients mapping $u_p$ to a Jump-Diffusion $u_G$, the system can actively price the probability of an overnight gap. If the PDE flags a high probability of a jump ($λ > \text{threshold}$), the strategy can dynamically widen the `--limit-offset` to execute the trade rather than strictly skipping it.

----
For the sake of clarity, let's replace $μ$ and $σ$ with $m$ and $s$ respectively in the Merton-Jump Diffusion pdf:

$$u_G(r,t) = \sum_{k=0}^{\infty} \frac{e^{-λ t} (λ t)^k}{k!} \cdot \frac{1}{\sqrt{2π (s^2 t + kδ^2)}} \exp\left( - \frac{(r - (m t + kγ))^2}{2(s^2 t + kδ^2)} \right)$$

The characteristic function (CF)—the Fourier transform of the PDF— of Merton-Jump Diffusion PDF has closed-form analytical solution:

$$φ(u, t) = \int^∞_{-∞} e^{i u r} u_G(r,t) dr = e^{ i u m t - \frac{1}{2} u^2 s^2 t + λ t \left( e^{i u γ - \frac{1}{2} u^2 δ^2} - 1 \right) }$$

To find the optimal parameters (μ,σ,λ,γ,δ), you generate a discrete grid of frequency points (e.g., u=0.1,0.5,1.0,2.0,…). For each point u on the grid, you minimize the squared distance 
between the theoretical $φ(u, t)$ CF of your target state and the empirical $\hatφ(u, t)$ CF of your actual historical data:

$$\min_{m,s,λ,γ,δ} \sum_u {\Vert φ(u, t) - \hat φ(u, t) \Vert}^2$$

where

$$\hat φ(u, t) = \frac{1}{N} \sum^N_{j=1}e^{i u r_j}$$

The  ${\Vert φ(u, t) - \hat φ(u, t) \Vert}^2$ can be reduced to a form without imaginary unit:

$$\Vert φ(u, t) - \hat{φ}(u, t) \Vert^2 = e^{2A(u,t)} + \left( \frac{1}{N} \sum_{j=1}^N \cos(u r_j) \right)^2 + \left( \frac{1}{N} \sum_{j=1}^N \sin(u r_j) \right)^2 - \frac{2 e^{A(u,t)}}{N} \sum_{j=1}^N \cos(u r_j - B(u,t))$$

where

$$A(u, t) = -\frac{1}{2} u^2 s^2 t + λ t \left( e^{-\frac{1}{2} u^2 δ^2} \cos(u γ) - 1 \right)$$

and

$$B(u, t) = u m t + λ t e^{-\frac{1}{2} u^2 δ^2} \sin(u γ)$$

----
The Cumulative Distribution Function (CDF) for the Merton Jump-Diffusion model is derived by integrating the probability density function (PDF) from $-\infty$ to $x$.

Because the integral operator is linear, you can pass the integral directly inside the infinite sum. The integral of each individual Gaussian PDF in the series simply becomes the standard Gaussian CDF.

The resulting exact CDF equation is:

$$U_G(x,t) = \sum_{k=0}^{\infty} \frac{e^{-λ t} (λ t)^k}{k!} Φ\left( \frac{x - (m t + kγ)}{\sqrt{s^2 t + k\delta^2}} \right)$$

Where:

* **$x$** is the target log-return threshold you are evaluating (e.g., your $x_{max}$ breakout level).
* **$Φ(\cdot)$** is the Cumulative Distribution Function of the Standard Normal distribution ($\mathcal{N}(0,1)$).
* **$m t + kγ$** is the expected mean return given exactly $k$ jumps.
* **$\sqrt{s^2 t + k\delta^2}$** is the total standard deviation given exactly $k$ jumps.

For implementation in your C++ solver, the standard Normal CDF $Φ(z)$ is computed using the error function (`std::erfc` or `std::erf`):

$$Φ(z) = \frac{1}{2} \text{erfc}\left( -\frac{z}{\sqrt{2}} \right)$$

Because the Poisson weight term $\frac{e^{-λ t} (λ t)^k}{k!}$ decays toward zero extremely rapidly for any realistic daily jump intensity ($λ$), you do not need to evaluate the infinite sum. In practice, truncating the loop at $k=10$ provides precision well beyond the standard `double` limit.

### 2. The Student's t-Distribution Setup (Dynamic Tail Expansion)

While the Generalized Hyperbolic (GH) distribution is the master equation for skew and kurtosis, a localized Student's t-distribution setup is highly efficient for modeling pure fat tails without the computational overhead of Bessel functions.

* **The Target ($u_G$):** The Student's t-distribution parameterized by degrees of freedom $\nu(t)$, which scales the fatness of the tails.

$$u_G(r,t) = \frac{\Gamma(\frac{\nu(t)+1}{2})}{σ(t) \sqrt{\nu(t) π} \Gamma(\frac{\nu(t)}{2})} \left( 1 + \frac{(r - μ(t))^2}{\nu(t) σ(t)^2} \right)^{-\frac{\nu(t)+1}{2}}$$


* **The Baseline ($u_p$):** The standard Gaussian distribution, which is the exact mathematical limit of the Student's t-distribution as $\nu \to \infty$.
* **Strategic Edge for One-Touch Options:** As market uncertainty increases, $\nu(t)$ decreases, shifting probability mass from the center into the extremes.

### 3. Extreme Value Theory (EVT) Setup (Threshold Exceedance)

For barrier options, the shape of the probability distribution near the mean ($μ$) is practically irrelevant. The only mathematical event that matters is whether the price exceeds the $x_{max}$ or $x_{min}$ threshold. EVT abandons the center mass entirely to model only the extremes.

* **The Target ($u_G$):** The Generalized Pareto Distribution (GPD), which models the probability density of returns strictly *given that they have already exceeded a high threshold* $u$.

$$u_G(r,t) = \frac{1}{\beta} \left( 1 + \xi \frac{r - u}{\beta} \right)^{-\left(\frac{1}{\xi} + 1\right)}$$


* **The Baseline ($u_p$):** The Exponential distribution, which is the proper subcase of the GPD when the shape parameter $\xi = 0$ (implying a light-tailed exponential decay of extreme returns).

$$u_p(x,t) = \frac{1}{\beta} e^{-\frac{x - u}{\beta}}$$


### 4. The Variance Gamma Setup (Business Time vs. Calendar Time)

Financial markets do not move in uniform clock time; they move in "business time" driven by trade volume and tick activity.

* **The Baseline ($u_p$):** A Gaussian distribution evaluating price over standard, uniform calendar time $t$.
* **The Target ($u_G$):** The Variance Gamma (VG) probability density function. It evaluates price as a continuous Brownian motion evaluated on a Gamma process (a randomized time clock).

$$u_G(r,t) = \int_{0}^{\infty} \frac{1}{σ \sqrt{2π g}} e^{-\frac{(r - μ - \theta g)^2}{2σ^2 g}} \cdot \frac{g^{\frac{t}{\nu} - 1} e^{-\frac{g}{\nu}}}{\nu^{\frac{t}{\nu}} \Gamma(\frac{t}{\nu})} dg$$


