import itertools
import numpy as np
from multiprocessing import cpu_count, Pool


def generate_points(n):
    n = int(n)
    return tuple([i * 1/n for i in range(-n, n)])


def lst_a(P):
    (b, c, d) = P
    d = d +  (1e-4 if d > 0 else -1e-4)
    a = (1+c*b)/d
    return (a, b, c, d)

def test_unit(P):
    (a, b, c, d) = P
    return np.round(a*d - b*c);

if __name__ == "__main__":
    N = 100    
    points = generate_points(N)
    pool = Pool(cpu_count())
    combinations = tuple(list(itertools.combinations(points, 3)))
    lst_space = pool.map(lst_a, combinations)

    