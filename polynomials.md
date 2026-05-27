## evaluate_horizontally ##
### Inputs ##

1. **tensor**: An instance of `td::mdspan` representing a sequence of 2D tables (`MacKinnonTable1x3x4`, `MacKinnonTable12x3x4`) as depicted below. The $p$, $m$ and $n$ represent 
the sequence length, matrix rows and columns respectively.

$
\begin{bmatrix}
a_{0,0,0} & a_{0,0,1} & ... & a_{1,0,n-1} \\
a_{0,1,0} & a_{0,1,1} & ... & a_{1,1,n-1} \\
      ... &       ... & ... &    ... \\
a_{0,m-1,0} & a_{0,m-1,1} & ... & a_{1,m-1,n-1}
\end{bmatrix}
$
...
$
\begin{bmatrix}
a_{p-1,0,0} & a_{p-1,0,1} & ... & a_{p-1,0,n-1} \\
a_{p-1,1,0} & a_{p-1,1,1} & ... & a_{p-1,1,n-1} \\
      ... &       ... & ... &    ... \\
a_{p-1,m-1,0} & a_{p-1,m-1,1} & ... & a_{p-1,m-1,n-1}
\end{bmatrix}
$.

For brevity we will represent this sequence as $A_0...A_p-1$.


2. **table_index**: Table index; this value ranges between $0$ and $p-1$.
4. **τ** Independent variable for polynomial evaluation.
3. **output** A `td::span` of size $m$. If evaluation fails, result is undefined; otherwise it will contain evaluation result defined as:

$A_i⋅𝐓$ where $𝐓$ is 
$\begin{bmatrix}
τ^0 \\
τ^2 \\
... \\
τ^{n-1}
\end{bmatrix}$

## evaluate_horizontally_reversed ##

This function is quite like `evaluate_horizontally_reversed` except for $𝐓$ vector which in this case is defined as:

$
\begin{bmatrix}
τ^{n-1} \\
τ^{n-2} \\
... \\
τ^0
\end{bmatrix}
$