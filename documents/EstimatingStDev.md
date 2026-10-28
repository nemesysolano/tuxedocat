

Here is how you actually use the $d_2$ table to estimate the standard deviation and variance from the extreme values of your sample.

1. **Determine your sample size:** Use n, not degrees of freedom.
The $d_2$ constants are indexed by the total number of items in your sample ($n$), not the degrees of freedom ($\nu$). If you have a sample where $\nu = 9$, your sample size is $n = 10$.


2. **Look up the d2 constant:** Reference the table below.
Find the row corresponding to your sample size $n$. This constant represents the expected value of the range for a standard normal distribution of that specific size.

| Sample Size ($n$) | $d_2$ Constant |
| --- | --- |
| 2 | 1.128 |
| 3 | 1.693 |
| 4 | 2.059 |
| 5 | 2.326 |
| 6 | 2.534 |
| 7 | 2.704 |
| 8 | 2.847 |
| 9 | 2.970 |
| 10 | 3.078 |
| 11 | 3.173 |
| 12 | 3.258 |
| 13 | 3.336 |
| 14 | 3.407 |
| 15 | 3.472 |


3. **Calculate the sample range:** High minus Low.
Subtract your lowest extreme value ($x_0$) from your highest extreme value ($x_1$) to find the range ($R$).


$$R = x_1 - x_0$$


4. **Estimate the standard deviation:** Divide the range by the constant.
Divide your calculated range by the $d_2$ constant. This gives you the point estimate for the population standard deviation ($\sigma$).


$$\sigma \approx \frac{R}{d_2}$$


5. **Square the result:** Final variance estimate.
Since variance ($\sigma^2$) is simply the standard deviation squared, square your result from the previous step.


$$\sigma^2 \approx \left(\frac{R}{d_2}\right)^2$$


If you had a sample of 10 items ($n=10$) where the lowest value was 45 and the highest was 85, your range is 40. You would divide 40 by 3.078 to get an estimated standard deviation of ~12.99. Squaring that gives you an estimated variance of ~168.86.