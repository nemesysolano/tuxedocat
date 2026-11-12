# Fractional Integral Transforms #

## Fractional Fourier Transform ##

The fractional Fourier transform of order $θ$ is defined as follows:

$\mathcal{F}_θ[f](x)= \hat{\mathcal{F}}_θ(x)=\int_{\mathbb{R}}{f(t) K_θ(x,t)dt}$.

Let's dissect the formula into its constituent parts

### The $K_θ(x,t)$ Kernel ###

$K_θ(x,t) =
\begin{cases}
    c(θ) e^{-i[a(θ) (t^2 + x^2)-b(θ) x t]},      &\text{ if } θ ≠ p π  \\
    δ(t-x),      & \text{ if } θ = 2 p π \\
    δ(t+x),      & \text{ if } θ = (2 p - 1) π 
\end{cases}
$ 

Furthermore 
- $a(θ) = \frac{\cot(θ)}{2}$, 
- $b(θ) = \csc (θ)$, 
- $c(θ) = \sqrt{\frac{1+ i \cot(θ)}{2 π}}$
- $d(θ) = \sqrt{1 + i \cot(θ)}$
- $p \in \mathbb{Z}$

### The Inverse Transform ###

$f(t)= \int_{\mathbb{R}}{\hat{\mathcal{F}}_θ(x) K_{-θ}(x,t)dx}$.

### Operational Properties ###

#### Linearity ####
For any constants $α, β \in \mathbb{C}$:

$\mathcal{F}_θ[α f + β g](x) = α \mathcal{F}_θ[f](x) + β \mathcal{F}_θ[g](x)$

#### Eigenvalues and Eigenfunctions ####

The normalized Hermite-Gaussian functions $h_n(x)$ are eigenfunctions of the fractional Fourier transform operator:
$\mathcal{F}_θ[h_n](x) = e^{inθ} h_n(x), \quad n \in \mathbb{N}_0$

where:
$h_n(x) = \frac{e^{-x^2/2}}{π^{1/4} \sqrt{2^n n!}} H_n(x)$ and

$H_n(x) = n! \sum\limits_{m=0}^{⌊n/2⌋} \frac{(-1)^m}{m!\,(n - 2m)!} (2x)^{n - 2m} = (-1)^n e^{x^2} \frac{d^n}{dx^n}(e^{-x^2})$ is the $n$-th Hermite polynomial (in explicit closed-form and Rodrigues' formula representations).


#### Additivity ####

$\mathcal{F}_θ \mathcal{F}_φ = \mathcal{F}_{θ+φ}$

#### Associativity ####

$\mathcal{F}_{θ_1} (\mathcal{F}_{θ_2} \mathcal{F}_{θ_3}) = (\mathcal{F}_{θ_1} \mathcal{F}_{θ_2}) \mathcal{F}_{θ_3}$

#### Commutativity ####

$\mathcal{F}_θ \mathcal{F}_φ = \mathcal{F}_φ \mathcal{F}_θ$

#### Modulation ####

$\mathcal{F}_θ[e^{i β t} f(t)](x) = e^{i β \cos(θ)\left(x + \frac{β}{2} \sin(θ)\right)} \mathcal{F}_θ[f](x + β \sin(θ))$

#### Differentiation ####

$\mathcal{F}_θ[f^{(n)}](x) = \left( \cos(θ)\frac{d}{dx} - i x \sin(θ) \right)^n \mathcal{F}_θ[f](x), \quad n \in \mathbb{N}$

where the power $n$ denotes $n$ successive applications of the operator $\left( \cos(θ)\frac{d}{dx} - i x \sin(θ) \right)$.

#### Fractional Convolution ####

Let

$\overset{↼}{f}(t) = f(t) e^{-i a(θ) t^2}, \quad \overset{↼}{g}(t) = g(t) e^{-i a(θ) t^2}$

The fractional convolution of order $θ$, denoted by $(f * g)(t)$, is defined as:

$h(t) = (f * g)(t) = d(θ) e^{i a(θ) t^2} (\overset{↼}{f} * \overset{↼}{g})(t)$

where $*$ denotes the standard classical convolution $(\overset{↼}{f} * \overset{↼}{g})(t) = \int_{\mathbb{R}} \overset{↼}{f}(τ) \overset{↼}{g}(t - τ) \, dτ$.

Then the fractional Fourier transform of $h(t)$ satisfies:

$e^{-i a(θ) x^2} \hat{\mathcal{H}}_θ(x) = \sqrt{2π} \hat{\mathcal{F}}_θ(x) \hat{\mathcal{G}}_θ(x)$

or equivalently:

$\hat{\mathcal{H}}_θ(x) = \sqrt{2π} e^{i a(θ) x^2} \hat{\mathcal{F}}_θ(x) \hat{\mathcal{G}}_θ(x)$

#### Fractional Product ####

For periodic signals, the product-type counterpart of the fractional convolution is the circular product. Let $f,g \in C([0,T])$ be continuous and $T$-periodic, and define the chirp-modified functions

$\overset{⇀}{f}(t) = f(t)e^{i a(θ)t^2}, \quad \overset{⇀}{g}(t) = g(t)e^{i a(θ)t^2}$.

Then the circular fractional product of order $θ$, denoted by $(f \otimes g)(t)$, is defined by

$(f \otimes g)(t) = d(-θ)e^{-i a(θ)t^2}\big(\overset{⇀}{f} * \overset{⇀}{g}\big)(t)$,

where $*$ denotes the usual (non-periodic) convolution on $\mathbb{R}$. In the periodic setting, this is equivalent to the circular convolution

$(f \otimes g)(t) = \int_0^T f(τ)\,g\big((t-τ)\bmod T\big)\,dτ$.

For a general positive period $T$, the modulo operation is defined by

$t \bmod T = t - T\left\lfloor\frac{t}{T}\right\rfloor$,

so that $t \bmod T \in [0,T)$. This definition applies equally to integer, rational, and real values of $T$. Thus, the circular product is well defined for any positive real period, provided the functions are interpreted on the periodic domain $\mathbb{R}/T\mathbb{Z}$.

This is a periodic product operation and should be distinguished from the more general fractional Fourier product theorem, which is usually formulated with chirp factors and fractional-domain kernels rather than a plain modulo-$T$ integral. In particular, the identity above is valid for periodic signals, whereas the general FRFT product law is not simply a circular convolution.

## Augmented Fractional Packet ##

Consider a strictly increasing sequence of angles $\{θ_0, \ldots, θ_{n-1}\}$ such that:

- $θ_i \in \left[\frac{1}{2} π, \frac{9}{10} π\right] \subset \mathbb{R} \setminus \{kπ : k \in \mathbb{Z}\}$ for every $i \in \{0, \ldots, n - 1\}$
- $θ_i < θ_{i+1}$ for every $i \in \{0, \ldots, n - 2\}$

Throughout, $x_t = \log\frac{P_t}{P_{t-1}}$ denotes the log return at time $t$, where $P_t$ is the close price. Each windowed realization $f_t$ is estimated exclusively from returns strictly before $t$ (see the Gaussian and Hyperbolic packets below), so $\mathcal{F}_θ[f_t](x_t)$ evaluates the newly observed return against a distribution that was known beforehand.

Log returns are tiny ($|x_t| \ll 1$) compared with the unit length scale implied by the kernel $K_θ$. Therefore, all transforms are taken in units of a fixed scale $s > 0$: $f_t$ is replaced by the density of the rescaled variable, $f_t^{(s)}(u) = s\,f_t(s u)$, and $x_t$ by $x_t / s$. In the system below, $f_k$ and $x_k$ stand for $f_k^{(s)}$ and $x_k / s$, respectively. The scale $s$ is the standard deviation of the $N_s$ log returns preceding the most recent observation of the system, using a slow window $N_s \gg N$ (e.g., $N_s = 360$, where $N$ is the length of the window used to estimate each $f_t$). It is shared by all equations of the system.

The scale must not be tied to the window of $f_t$ itself. If each $f_t$ were standardized by its own $σ_t$, it would be the Hermite-Gaussian $h_0$, which is a fixed point of every $\mathcal{F}_θ$, and the dependence on $θ$ would vanish. With a slow $s$, the ratio $σ_t / s$ measures the current volatility regime and makes the dependence on $θ$ non-trivial.

Additionally, let $f$ be an integrable function (with windowed realizations $f_t$), and suppose that for $m > n$ the following system of equations admits a solution in the complex coefficients $α_0, \ldots, α_{n-1} \in \mathbb{C}$:

$$
\begin{cases}
\begin{aligned}
    α_0 \mathcal{F}_{θ_0}[f_0](x_0) + α_1 \mathcal{F}_{θ_1}[f_1](x_1) + \cdots + α_{n-1} \mathcal{F}_{θ_{n-1}}[f_{n-1}](x_{n-1}) &= \mathcal{F}[f_n](x_n) \\
    α_0 \mathcal{F}_{θ_0}[f_n](x_n) + α_1 \mathcal{F}_{θ_1}[f_{n + 1}](x_{n + 1}) + \cdots + α_{n-1} \mathcal{F}_{θ_{n-1}}[f_{2n - 1}](x_{2n - 1}) &= \mathcal{F}[f_{2n}](x_{2n}) \\
    α_0 \mathcal{F}_{θ_0}[f_{2n}](x_{2n}) + α_1 \mathcal{F}_{θ_1}[f_{2n + 1}](x_{2n + 1}) + \cdots + α_{n-1} \mathcal{F}_{θ_{n-1}}[f_{3n - 1}](x_{3n - 1}) &= \mathcal{F}[f_{3n}](x_{3n}) \\
    \cdots \\
    α_0 \mathcal{F}_{θ_0}[f_{(m-1)n}](x_{(m-1)n}) + α_1 \mathcal{F}_{θ_1}[f_{(m-1)n + 1}](x_{(m-1)n + 1}) + \cdots + α_{n-1} \mathcal{F}_{θ_{n-1}}[f_{mn - 1}](x_{mn - 1}) &= \mathcal{F}[f_{mn}](x_{mn}) \\
\end{aligned}
\end{cases}
$$

In that case, we define the **augmented fractional packet** as the tuple $\{f, α_0, \ldots, α_{n-1}\}$ composed of $f$ and the coefficients $α_0, \ldots, α_{n-1}$.

#### Inference at $mn + 1$ ####

Let $A \in \mathbb{C}^{m \times n}$ denote the system matrix with entries $A_{r, j} = \mathcal{F}_{θ_j}[f_{rn + j}](x_{rn + j})$ and $\mathbf{b} \in \mathbb{C}^m$ denote the right-hand side vector with entries $b_r = \mathcal{F}[f_{(r+1)n}](x_{(r+1)n})$ for $r \in \{0, \ldots, m - 1\}$. Every entry of $A$ and $\mathbf{b}$ is known once session $mn$ closes.

When the least-squares solution $\boldsymbol{α} = (α_0, \ldots, α_{n-1})^T \in \mathbb{C}^n$ of $A\boldsymbol{α} \approx \mathbf{b}$ is finite and achieves a coefficient of determination

$$R^2 = 1 - \frac{\Vert A\boldsymbol{α} - \mathbf{b} \Vert_2^2}{\Vert \mathbf{b} - \bar{b}\mathbf{1} \Vert_2^2} > τ$$

(for a prescribed coherence threshold $τ \in (0, 1)$, such as $τ = 0.95$), the sequence of densities up to $f_{mn}$ is validated as phase-coherent across the fractional orders $\{θ_0, \ldots, θ_{n-1}\}$. We then use $f_{mn}$ to make inference about the next log return $x_{mn + 1}$ via the cumulative probability of the realized return $x_{mn}$:

$$p = \int_{-\infty}^{x_{mn}} f_{mn}(ξ) \, dξ$$

Specifically, an upper-tail exceedance ($p > 0.95$) supports a positive continuation (`LONG`) at $mn + 1$, a lower-tail exceedance ($p < 0.05$) supports a negative continuation (`SHORT`) at $mn + 1$, and any outcome where the system fails the coherence threshold ($R^2 \le τ$) or $p \in [0.05, 0.95]$ yields no directional signal (`IDLE`).

### Gaussian Fractional Packet ###

Suppose that each $f_t$ is a Gaussian probability density function with location and scale parameters $μ_t$ and $σ_t$, respectively, estimated over the $N$ log returns preceding $t$ as:

$μ_t = \frac{1}{N} \sum\limits_{w=1}^{N} x_{t-w}$, and

$σ_t = \sqrt{\frac{1}{N - 1}\sum\limits_{w=1}^{N}(x_{t-w} - μ_t)^2}$.

In this scenario, we obtain a **Gaussian fractional packet**, which is an instance of an augmented fractional packet. Because $f_t(u) = \frac{1}{σ_t \sqrt{2π}} \exp\left(-\frac{(u - μ_t)^2}{2σ_t^2}\right)$ is Gaussian, the fractional Fourier transform $\mathcal{F}_θ[f_t](x_t)$ (with $x_t, μ_t, σ_t$ expressed in units of the scale $s$) has the exact analytical closed form:

$$\mathcal{F}_θ[f_t](x_t) = \frac{c(θ)}{\sqrt{1 + 2i a(θ) σ_t^2}} \exp\left( \frac{-i a(θ)(x_t^2 + μ_t^2) + i b(θ) μ_t x_t - \frac{1}{2} σ_t^2 x_t^2}{1 + 2i a(θ) σ_t^2} \right)$$

where $a(θ) = \frac{\cot(θ)}{2}$, $b(θ) = \csc(θ)$, and $c(θ) = \sqrt{\frac{1 + i\cot(θ)}{2π}}$. On the right-hand side of the system ($θ = \frac{π}{2}$, where $a(\frac{π}{2}) = 0$, $b(\frac{π}{2}) = 1$, and $c(\frac{π}{2}) = \frac{1}{\sqrt{2π}}$), this reduces to the classical Fourier transform of $f_t$:

$$\mathcal{F}[f_t](x_t) = \frac{1}{\sqrt{2π}} \exp\left( i μ_t x_t - \frac{1}{2} σ_t^2 x_t^2 \right)$$

### Hyperbolic Fractional Packet ###

Evaluating $\mathcal{F}_{θ_j}[f_{h,t}](x_t)$ directly on the **Generalized Hyperbolic probability density function** $f_{h,t}$ (defined in [Augmented-Non-Linear-Wave-Packet](Augmented-Non-Linear-Wave-Packet.md)) for $θ_j \neq \frac{π}{2}$ couples the quadratic chirp $e^{-i a(θ_j) u^2}$ with the modified Bessel function $K_{λ - 1/2}\!\left(α_t \sqrt{σ_t^2 + (u - μ_t)^2}\right)$, which admits no closed-form antiderivative and would require numerical quadrature for every matrix entry $A_{r,j}$.

Instead, following the **Particular-to-General** transfer architecture of [particular-to-general](particular-to-general.md), we formulate the **Particular-to-General Hyperbolic Fractional Packet**:

- **Left-hand side (Particular / Gaussian Baseline)**: Each matrix entry $A_{r,j} = \mathcal{F}_{θ_j}[f_{g, rn + j}](x_{rn + j})$ transforms the Gaussian baseline density $f_{g,k}$ using the exact closed form above.
- **Right-hand side (General / Hyperbolic Target)**: Each target entry $b_r = \mathcal{F}[f_{h, (r+1)n}](x_{(r+1)n})$ evaluates the classical Fourier transform ($θ = \frac{π}{2}$) of the Generalized Hyperbolic density $f_{h,t}$:

$$f_{h,t}(u) = c_t \left( σ_t^2 + (u - μ_t)^2 \right)^{\frac{λ - 1/2}{2}} K_{λ - 1/2}\!\left( α_t \sqrt{σ_t^2 + (u - μ_t)^2} \right) e^{β_t(u - μ_t)}$$

At $θ = \frac{π}{2}$, $\mathcal{F}[f_{h,t}](x_t)$ is $\frac{1}{\sqrt{2π}}$ times the Generalized Hyperbolic characteristic function, which has the exact analytical closed form:

$$\mathcal{F}[f_{h,t}](x_t) = \frac{e^{i μ_t x_t}}{\sqrt{2π}} \left( \frac{α_t^2 - β_t^2}{α_t^2 - (β_t + i x_t)^2} \right)^{\frac{λ}{2}} \frac{K_λ\!\left(σ_t \sqrt{α_t^2 - (β_t + i x_t)^2}\right)}{K_λ\!\left(σ_t \sqrt{α_t^2 - β_t^2}\right)}$$

For the Normal Inverse Gaussian specialization ($λ = -\frac{1}{2}$, where $K_{-1/2}(z) = \sqrt{\frac{π}{2z}} e^{-z}$), the Bessel functions cancel identically and the right-hand side reduces to an elementary, Bessel-free closed form:

$$\mathcal{F}[f_{h,t}](x_t) = \frac{1}{\sqrt{2π}} \exp\!\left( i μ_t x_t + σ_t \left( \sqrt{α_t^2 - β_t^2} - \sqrt{α_t^2 - (β_t + i x_t)^2} \right) \right)$$

(Under the scale $s$, replacing $x_t \mapsto x_t / s$, $μ_t \mapsto μ_t / s$, $σ_t \mapsto σ_t / s$, $α_t \mapsto s α_t$, and $β_t \mapsto s β_t$ leaves all products inside the exponent invariant.)

#### Advantages Over the Generalized Hyperbolic Wave Packet ####

1. **$O(1)$ Closed-Form Matrix Assembly (No Bessel Calls for $λ = -\frac{1}{2}$)**: Both $A_{r,j}$ and $b_r$ are evaluated via elementary complex arithmetic, avoiding `cyl_bessel_k` evaluations when building the linear system and requiring numerical quadrature only once at the final CDF inference step $p = \int_{-\infty}^{x_{mn}} f_{h,mn}(ξ)\,dξ$ when $R^2 > τ$.
2. **Global Moment & Tail Encoding**: Rather than matching the density height $f_{h,t}(x_t)$ at a single point, $\mathcal{F}[f_{h,t}](x_t)$ matches the characteristic function of the target distribution, capturing location $μ_t$, scale $σ_t$, skewness $β_t$ (in the complex phase), and heavy-tail steepness $α_t$ (in the radical damping) simultaneously.
3. **Unconditional Tail Boundedness**: Because $\left|\mathcal{F}[f_{h,t}](x_t)\right| \le \frac{1}{\sqrt{2π}}$ for all $x_t \in \mathbb{R}$, extreme return shocks cannot trigger the polynomial or rational derivative blowups ($u^3$, $u_x / u$, $u_{xx} / u$) present in the non-linear wave packet PDE.