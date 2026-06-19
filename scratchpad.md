# Logistic Regression (Log Return instead vs Pct Change) #

Yes, absolutely. In fact, transitioning from simple percentage changes to log returns—also known as continuously compounded returns—is a standard best practice in quantitative finance, especially when expanding lookback windows to 20 or 30 days.

Using $r_t = \ln(P_t / P_{t-1})$ instead of $R_t = \frac{P_t - P_{t-1}}{P_{t-1}}$ fixes several mathematical issues that machine learning algorithms struggle with when processing raw financial data.

Here is why this swap will immediately improve the mathematical stability and predictive power of your model:

## 1. Time Additivity (The Core Advantage)

Simple returns do not aggregate cleanly across time. If a stock drops 50% today and gains 50% tomorrow, your total return is not 0%; you are down 25%. This geometric compounding makes it difficult for linear models to assess the true trajectory over a 30-day period.

Log returns are strictly additive.
$\ln(\frac{P_t}{P_{t-2}}) = \ln(\frac{P_t}{P_{t-1}}) + \ln(\frac{P_{t-1}}{P_{t-2}})$

If you want to know the total return over the 30-day window, you simply sum the 30 daily log returns. Because Logistic Regression relies on linear combinations of features ($z = \beta_0 + \beta_1 X_1 + \dots$), feeding it time-additive log returns aligns perfectly with how the algorithm processes weights.

## 2. Symmetry and the Normal Distribution

Simple returns are heavily skewed: they are bounded at -100% on the downside but have infinite upside. This creates an asymmetric, log-normal distribution.

Log returns are symmetric. A +10% log return exactly neutralizes a -10% log return. This symmetry pulls the distribution of your features much closer to a normal (Gaussian) distribution. Iteratively Reweighted Least Squares (IRLS) converges much faster and more reliably when the input features are normally distributed.

## 3. Built-in Outlier Compression (Numerical Stability)

In the C++ implementation, it was necessary to add `std::clamp` to the logit calculation to prevent the exponential function from overflowing due to extreme lag values. Logarithms naturally compress large positive outliers. By compressing the extreme right tail of your returns, log returns act as a natural scaling mechanism, keeping your feature matrix stable and preventing the gradient from blowing up during matrix inversion.

## Architectural Advice for a 30-Day Lookback

Expanding from 5 days to 30 days introduces a new challenge: **The Curse of Dimensionality**. If you feed 30 separate daily lag columns into the Logistic Regression, the model has to optimize 31 coefficients. This increases the risk of the model fitting to random daily noise rather than the actual trend, requiring a massive amount of rows to avoid overfitting.

Instead of passing 30 individual daily lags, you can exploit the time-additivity of log returns to create **aggregated horizon features**.

Instead of $X = [\text{Lag}_1, \text{Lag}_2, \dots, \text{Lag}_{30}]$, you can engineer features that represent different market memory lengths:

* **1-Day Momentum:** $\ln(P_t / P_{t-1})$
* **1-Week Momentum:** $\ln(P_t / P_{t-5})$ *(which is just the sum of the last 5 daily log returns)*
* **2-Week Momentum:** $\ln(P_t / P_{t-10})$
* **1-Month Momentum:** $\ln(P_t / P_{t-21})$

This approach compresses 30 days of historical data into just 4 highly distinct, uncorrelated features. It allows the model to "see" the 30-day trend without starving the IRLS solver for degrees of freedom.


## Transitioning from Classification to Probability
Right now, your predict function classifies a day as Positive (1.0) if the underlying logistic equation $z ≥ 0$, which corresponds to a probability of $≥50%$.
Since you are not forced to trade every single day, you can exploit the logistic curve to improve your precision. Logistic Regression doesn't just output a binary classification; it outputs a confidence probability: 

$p= \frac{1}{1+\exp^{-z}}$

If you modify your predict function to return the actual probability p instead of just 1.0 or -1.0, you can build a statistical arbitrage execution pipeline that only takes trades when the model is highly confident. For example, your signal logic could dictate:

- Go Long: If $p > 0.55$
- Go Short: If $p<0.45 $
- Stay in Cash: If $0.45 ≤ p ≤0.55$

By filtering out the low-confidence predictions (where the model is essentially flipping a coin), you will likely see your precision and overall backtest Sharpe ratio climb significantly, trading slightly less frequently but with a much higher win rate.


# Linear Discriminant Analysis (LDA) #

Switching from Logistic Regression to **Linear Discriminant Analysis (LDA)** is a fantastic architectural pivot, especially in quantitative finance. While Logistic Regression is a *discriminative* model (it only cares about finding the boundary between classes), LDA is a *generative* model. It tries to understand the actual distribution of the data in each class before drawing a line between them.

Here is a breakdown of how LDA works, the mathematical intuition, and its core formulae.

## The Core Intuition of LDA

Imagine you have a scatter plot of your 5-day momentum features. The positive return days are mixed in with the negative return days. LDA tries to find a new axis (a 1D line) to project this data onto so that two things happen simultaneously:

1. **Maximize the Distance Between Means:** The center of the "Positive Days" cluster should be as far away as possible from the center of the "Negative Days" cluster.
2. **Minimize the Scatter Within Classes:** The points within the "Positive Days" cluster should be grouped as tightly together as possible (low variance), and the same for the negative days.

This is known as the **Fisher Criterion**. By doing both, you minimize the overlap between the two classes.

---

## The Mathematics and Formulae

Assume you have a dataset with $N$ total samples, split into two classes: Class 1 ($C_1$, Positive Returns) and Class 2 ($C_2$, Negative Returns). Your feature matrix $X$ has $d$ dimensions (e.g., your momentum horizons).

### Step 1: Calculate Class Means

First, find the average vector for each class.


$$\mu_1 = \frac{1}{N_1} \sum_{x \in C_1} x$$

$$\mu_2 = \frac{1}{N_2} \sum_{x \in C_2} x$$

### Step 2: Calculate Within-Class Scatter ($S_W$)

How spread out is the data within its own class? We calculate the scatter matrix (which is proportional to the covariance matrix) for each class and sum them.


$$S_1 = \sum_{x \in C_1} (x - \mu_1)(x - \mu_1)^T$$

$$S_2 = \sum_{x \in C_2} (x - \mu_2)(x - \mu_2)^T$$

$$S_W = S_1 + S_2$$

### Step 3: Calculate Between-Class Scatter ($S_B$)

How far apart are the two class means? This is represented by the outer product of the difference between the mean vectors.


$$S_B = (\mu_1 - \mu_2)(\mu_1 - \mu_2)^T$$

### Step 4: Maximize the Fisher Criterion

We are looking for a projection vector $w$ (our coefficients) that maximizes the ratio of between-class scatter to within-class scatter. The objective function is:


$$J(w) = \frac{w^T S_B w}{w^T S_W w}$$

### Step 5: The Optimal Solution (The Weights)

Using calculus and linear algebra (specifically solving the generalized eigenvalue problem), the optimal projection vector $w$ that maximizes $J(w)$ has a beautifully simple closed-form solution:


$$w \propto S_W^{-1} (\mu_1 - \mu_2)$$

This is your model! $S_W^{-1}$ is the inverse of the within-class scatter matrix, and $(\mu_1 - \mu_2)$ is the vector pointing from one mean to the other.

Once you calculate $w$, predicting a new day's direction is just a dot product: $z = w^T x$. If $z$ is greater than a certain threshold, you classify it as positive.

---

## LDA vs. Logistic Regression for Trading

* **No Iterations Required:** Unlike Logistic Regression which requires the IRLS algorithm to loop 10 times to find the weights, LDA is a closed-form analytical solution. It is just matrix multiplication and one matrix inversion ($S_W^{-1}$). It runs infinitely faster, which is great for high-frequency backtesting.
* **The Normality Assumption:** LDA strictly assumes that your features are Normally Distributed (Gaussian) and that both classes share the exact same covariance matrix (Homoscedasticity). Because you are now using **log returns** (which are highly symmetric and normally distributed), LDA is practically tailor-made for your current dataset.

To help solidify the geometric intuition behind the math, here is an interactive visualization of how the projection angle affects the Fisher Criterion.

# Quadratic Discriminant Analysis (QDA) #

In LDA, the math explicitly assumes that the "Up" days and the "Down" days share the exact same structural shape (Covariance Matrix, $Σ$). LDA assumes that the spread, volatility, and correlation of your features are identical regardless of whether the market goes up or down tomorrow.

QDA drops the equal variance assumption. It allows each class to have its own distinct covariance matrix ($Σ_{\text up}$ and $Σ_{\text down}$). Because the model now understands that the "Up" class has a different geometric shape than the "Down" class, it no longer draws a straight line. Instead, it wraps the classes in curves (parabolas, ellipses, or hyperbolas).

## The Discriminant Function $δ_k(x)$ ##

Let $k$ a class (either "up" or "down"), then we define $δ_k(x)$

$δ_k(x) = -\frac{1}{2} \log |Σ_k| - \frac{1}{2} (x - μ_k)^T Σ_k^{-1} (x - μ_k) + \log π_k$, where

1. $Σ_k$: Covariance matrix for class $k$.
2. $|Σ_k|$: Determinant of $Σ_k$.
3. $Σ_k^{-1}$: Inverse $Σ_k$
4. $μ_k$: Mean vector for class $k$
5. $π_k$: Ratio of samples in class $k$ to total.


To make a prediction  $δ_{up}(x)$ and $δ_{down}(x)$. Whichever value is larger dictates the prediction.

# Radial Support Vector Machine (RSVM) #

To understand the Radial Support Vector Machine (RBF SVM), overcomes the fundamental limitation of linear models. Linear Discriminant Analysis (LDA), Logistic Regression, and Linear Support Vector Classifiers all try to separate data by drawing straight lines or flat planes.

## The Math: The Radial Basis Function (RBF) ##
The engine that performs this dimensional warping is the Radial Basis Function (RBF) Kernel.
Rather than comparing the features of two data points via a standard dot product, the RBF kernel measures the **Euclidean distance** between them to calculate a similarity score.

The kernel formula between any two data points $x_i$ and $x_j$ is:

$K(x_i, x_j) = \exp(-γ ||x_i - x_j||^2)$

## The Prediction Function (Decision Boundary) ##

Once the model is trained, it does not use a global weight vector to make predictions. Instead, it memorizes a critical subset of your training data known as the Support Vectors.To predict the class of a new, unseen data point $x$, the SVM calculates its RBF similarity to all the memorized Support Vectors and takes a weighted vote:

$f(x) = \text{sign}\left( \sum_{i=1}^{M} \alpha_i y_i \exp(-\γ ||x_i - x||^2) + b \right)$

- $M$: The total number of historical points.
- $y_i$: The actual label of the historical point (e.g., +1 or -1).
- $α_i$: The specific voting weight assigned to that historical point.
- $b$: The bias term.

## The Training Phase and the Penalty ($C$) ##

During training, the algorithmic solver tries to maximize the margin between classes while assigning the $\alpha_i$ weights. However, it is strictly bound by a ceiling:

$0 \leq \alpha_i \leq C$