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
$h_n(x) = \frac{e^{-x^2/2}}{\pi^{1/4} \sqrt{2^n n!}} H_n(x)$ and $H_n(x)$ is the $n$-th Hermite polynomial.


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

$$\tilde{f}(t) = f(t) e^{-i a(θ) t^2}, \quad \tilde{g}(t) = g(t) e^{-i a(θ) t^2}$$

The fractional convolution of order $θ$, denoted by $(f *_\theta g)(t)$, is defined as:

$$h(t) = (f *_\theta g)(t) = d(θ) e^{i a(θ) t^2} (\tilde{f} * \tilde{g})(t)$$

where $*$ denotes the standard classical convolution $(\tilde{f} * \tilde{g})(t) = \int_{\mathbb{R}} \tilde{f}(\tau) \tilde{g}(t - \tau) \, d\tau$.

Then the fractional Fourier transform of $h(t)$ satisfies:

$$e^{-i a(θ) x^2} \hat{\mathcal{H}}_θ(x) = \sqrt{2\pi} \hat{\mathcal{F}}_θ(x) \hat{\mathcal{G}}_θ(x)$$

or equivalently:

$$\hat{\mathcal{H}}_θ(x) = \sqrt{2\pi} e^{i a(θ) x^2} \hat{\mathcal{F}}_θ(x) \hat{\mathcal{G}}_θ(x)$$
