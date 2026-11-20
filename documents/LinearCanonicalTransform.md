# The Linear Canonical Transform (LCT) #
Let $f \in L^1(ℝ) ⋂ L^2(ℝ)$ and $a$, $b$, $c$ and $d$ be four real numbers belonging to $M$ matrix defined as as 

$ M =
\begin{pmatrix}
a & b \\
c & d
\end{pmatrix}
$, 
where $\det(M) = 1 $

The liner cananonical transform of $f$ associated to $M$ matrix is defined as

$ℒ_M[f](t)  = \begin{cases}
  \frac{1}{\sqrt {2 π i b}} \int_ℝ f(x) e^{\frac{i}{2 b}[a x^2 -2 x t + d t^2]} dx & \text{if } b ≠ 0 \\
  \frac{e^{\frac{i c t^2}{2 a}}}{\sqrt{a}} f(t/a) & \text{if } b = 0
\end{cases}
$

## Function Spaces $L^1(ℝ)$ and $L^2(ℝ)$ ##

- **$L^1(ℝ)$ (Space of Absolutely Integrable Functions):** Consists of all Lebesgue-measurable functions $f : ℝ → ℂ$ whose total absolute area is finite:

  $$\|f\|_{L^1} = \int_{ℝ} |f(x)| \, dx < \infty$$

  Because the LCT kernel has unit modulus $\left|e^{\frac{i}{2b}[ax^2 - 2xt + dt^2]}\right| = 1$, assuming $f \in L^1(ℝ)$ guarantees that the improper integral $ℒ_M[f](t)$ converges absolutely for every $t \in ℝ$ and defines a continuous, bounded function vanishing at infinity ($ℒ_M[f] \in C_0(ℝ)$). Every probability density function $f \ge 0$ automatically belongs to $L^1(ℝ)$ with $\|f\|_{L^1} = 1$.

- **$L^2(ℝ)$ (Space of Square-Integrable / Finite-Energy Functions):** The Hilbert space of Lebesgue-measurable functions $f : ℝ → ℂ$ with finite total energy:

  $$\|f\|_{L^2} = \left(\int_{ℝ} |f(x)|^2 \, dx\right)^{1/2} < \infty, \qquad \langle f, g \rangle_{L^2} = \int_{ℝ} f(x)\,\overline{g(x)} \, dx$$

  On $L^2(ℝ)$, the Linear Canonical Transform extends uniquely to a **unitary operator**, preserving inner products ($\langle ℒ_M[f], ℒ_M[g] \rangle = \langle f, g \rangle$), preserving energy (**Parseval/Plancherel theorem**: $\|ℒ_M[f]\|_{L^2} = \|f\|_{L^2}$), and admitting an exact inverse $ℒ_{M^{-1}}$ via $M^{-1} = \begin{pmatrix} d & -b \\ -c & a \end{pmatrix}$.

- **Why $f \in L^1(ℝ) \cap L^2(ℝ)$:** Requiring $f$ to lie in the intersection guarantees both **pointwise convergence** of the direct Lebesgue integral formula ($L^1$) and **unitary invertibility / energy conservation** ($L^2$). Notably, neither space contains the other on $ℝ$ (e.g., $(1 + |x|)^{-1} \in L^2(ℝ) \setminus L^1(ℝ)$, while $|x|^{-1/2}\mathbf{1}_{|x| \le 1} \in L^1(ℝ) \setminus L^2(ℝ)$), whereas smooth, bounded, rapidly decaying densities such as the Gaussian and Generalized Hyperbolic PDFs lie in $L^1(ℝ) \cap L^2(ℝ)$.

## General Fractional Packet ##

Consider the general fractional packet relation, where $ℒ_{M_i}[f_i](x_i)$ denotes the Linear Canonical Transform associated with a unimodular matrix $M_i \in SL(2, ℝ)$ ($\det(M_i) = 1$), and $\mathcal{F}[f_{n-1}](x_n)$ denotes the classical Fourier transform:

$$α_0 ℒ_{M_0}[f_0](x_0) + α_1 ℒ_{M_1}[f_1](x_1) + \cdots + α_{n-1} ℒ_{M_{n-1}}[f_{n-1}](x_{n-1}) = \mathcal{F}[f_{n-1}](x_n)$$

Depending on whether the parameter matrices $\{M_i\}_{i=0}^{n-1}$ are fixed or learned from data, estimation splits into two distinct regimes:

1. **Fixed Matrices $\{M_i\}$ (Linear Estimation via OLS):**
   When the matrices $M_0, \dots, M_{n-1}$ are held constant (for instance, chosen from a pre-specified dictionary or along the Fractional Fourier rotation family), each transformed term $ℒ_{M_i}[f_i](x_i)$ is a known constant for a given observation window. Across $m > n$ shifted blocks, the system is strictly linear in the complex weight vector $α = (α_0, \dots, α_{n-1})^T \in ℂ^n$, so Ordinary Least Squares (via QR or SVD) yields the exact global least-squares solution.

2. **Joint Estimation of Both $α$ and $\{M_i\}$ (Why OLS Is Not Suitable):**
   If the objective is to estimate both the coefficients $α_i$ and the $SL(2, ℝ)$ matrices $M_i$ simultaneously, standard OLS is no longer applicable or effective for three reasons:
   - **Nonlinearity in $(a_i, b_i, d_i)$:** The transform parameters enter $ℒ_{M_i}[f_i](x_i)$ nonlinearly inside both the complex scaling factor $\frac{1}{\sqrt{2π i b_i}}$ and the oscillatory quadratic chirp $\exp\!\left(\frac{i}{2b_i}[a_i u^2 - 2u x_i + d_i x_i^2]\right)$.
   - **Non-Convex Manifold Constraint:** Unconstrained OLS searches over Euclidean space, whereas each matrix must satisfy $\det(M_i) = a_i d_i - b_i c_i = 1$ on the curved manifold $SL(2, ℝ)$, while also handling the singular branch transition at $b_i = 0$.
   - **Oscillatory Local Minima and Over-Parameterization:** Fitting $n$ complex weights alongside $3n$ continuous $SL(2, ℝ)$ parameters makes the objective non-convex and substantially increases sample complexity ($m \gg 4n$) to avoid overfitting.

   Instead, joint estimation is better addressed via **Variable Projection** (using OLS/QR for the conditionally linear inner solve $α M $ wrapped inside an outer nonlinear or manifold optimizer such as Levenberg–Marquardt over $\{M_i\}$), or by expanding over a fixed $SL(2, ℝ)$ basis dictionary solved with regularized linear regression (such as Ridge or Lasso).