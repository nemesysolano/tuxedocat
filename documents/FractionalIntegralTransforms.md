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

$\tilde{f}(t) = f(t) e^{-i a(θ) t^2}, \quad \tilde{g}(t) = g(t) e^{-i a(θ) t^2}$

The fractional convolution of order $θ$, denoted by $(f * g)(t)$, is defined as:

$h(t) = (f * g)(t) = d(θ) e^{i a(θ) t^2} (\tilde{f} * \tilde{g})(t)$

where $*$ denotes the standard classical convolution $(\tilde{f} * \tilde{g})(t) = \int_{\mathbb{R}} \tilde{f}(τ) \tilde{g}(t - τ) \, dτ$.

Then the fractional Fourier transform of $h(t)$ satisfies:

$e^{-i a(θ) x^2} \hat{\mathcal{H}}_θ(x) = \sqrt{2π} \hat{\mathcal{F}}_θ(x) \hat{\mathcal{G}}_θ(x)$

or equivalently:

$\hat{\mathcal{H}}_θ(x) = \sqrt{2π} e^{i a(θ) x^2} \hat{\mathcal{F}}_θ(x) \hat{\mathcal{G}}_θ(x)$
