import argparse
import sys
import collections
import math
import numpy as np
import random
import scipy.stats

parser = argparse.ArgumentParser()
parser.add_argument('--delta', type=float, default=0.001)
parser.add_argument('--threshold', type=float, default=0.005)
parser.add_argument('--iterations', type=int, default=1000000)
parser.add_argument('--num_stocks', type=int, default=2)
parser.add_argument('--remove_common_flips', type=bool, default=False)

class Flipper:
    def __init__(self, delta, threshold):
        self.delta = delta
        self.threshold = threshold
        self.prices = [1.0 for _ in range(args.num_stocks)]
        self.bh = [1.0 for _ in range(args.num_stocks)]
        self.wave = [1.0 for _ in range(args.num_stocks)]
        self.last_g = None
        self.cur_g = None
        self.min_g_delta = None

    def __str__(self):
        fmt_list = lambda l: '[' + (', '.join(['{:.3f}'.format(val) for val in l])) + ']'
        return (
                'Flipper(prices={}, bh=({:.3f}), wave={}({:.3f}), ratio={:.3f}, g_bh={:.3f}, g_wave={:.3f})'.format(
                    fmt_list(self.prices), self.total_value(self.bh),
                    fmt_list(self.wave), self.total_value(self.wave),
                    self.total_value(self.wave) / self.total_value(self.bh),
                    self.g(self.bh), self.g(self.wave)))

    def flip(self):
        if False:
            mu = 0.0
            sigma = 0.015
            dt = 1.0 / 252

            drift = mu * dt
            for i, price in enumerate(self.prices):
                shock = sigma * np.random.normal(0, 1) * np.sqrt(dt)
                p = price + price * (drift + shock)
                self.prices[i] = p
        else:
            if args.remove_common_flips:
                #flips = [1, 0] if np.random.binomial(1, 0.5) else [0, 1]
                flips = [1, 0] if random.randint(0, 1) else [0, 1]
            else:
                assert(False)
                flips = np.random.binomial(1, 0.5, args.num_stocks)
            self.prices = [
                    val * (1.0 + self.delta) if flip else val / (1.0 + self.delta)
                    for flip, val in zip(flips, self.prices)]

    def flip_direction(self, up: bool):
        if up:
            flips = [1, 0]
        else:
            flips = [0, 1]

        self.prices = [
                val * (1.0 + self.delta) if flip else val / (1.0 + self.delta)
                for flip, val in zip(flips, self.prices)]

    def excess(self):
        return int(np.round(math.log(self.prices[0], 1.0 + self.delta)))

    def successes(self, n_flips):
        return n_flips // 2 + self.excess() // 2

    def probability_density(self, n_flips):
        return scipy.stats.binom(n_flips, 0.5).pmf(self.successes(n_flips))

    def weighted_value(self, n_flips):
        value = self.g(self.bh)
        weight = self.probability_density(n_flips)
        return value * weight

    def total_value(self, positions):
        return sum([w * v for w, v in zip(positions, self.prices)])

    def g(self, positions):
        total = self.total_value(positions)
        return np.power(
                np.product([total / price for price in self.prices]),
                1 / args.num_stocks)

    def rebalance(self):
        total = self.total_value(self.wave)
        self.wave = [total / args.num_stocks / val for val in self.prices]

    def simulate(self, samples):
        for i in range(samples):
            self.flip()
            #frac = self.total_value(self.wave) / args.num_stocks
            #vals = [price * size for price, size in zip(self.prices, self.wave)]

            #if max(vals) / frac - 1.0 > self.threshold:
            #    self.rebalance()


if __name__ == '__main__':
    args = parser.parse_args()

    if False:  # Generic stocks from commandline.
        flipper = Flipper(args.delta, args.threshold)
        flipper.simulate(args.iterations)
        print(flipper, file=sys.stderr)
    elif False:  # Just two stocks.
        bh_results = []
        wave_results = []
        print(args)
        trials = 100
        successes = []
        for i in range(trials):
            flipper = Flipper(args.delta, args.threshold)
            flipper.simulate(args.iterations)
            bh_results.append(flipper.g(flipper.bh))
            wave_results.append(flipper.g(flipper.wave))
            print('progress: {} / {} -- {}(min: {}, max: {}, recent: {}), {}, excess: {}\n'.format(i, trials, np.mean(bh_results), min(bh_results), max(bh_results), bh_results[-1], np.mean(wave_results), flipper.excess()), end='')
            sys.stdout.flush()
            successes.append(flipper.successes(args.iterations))
        print()
        print(np.mean(bh_results), np.mean(wave_results))
        print(sum(successes))
        print(f"Mean: {np.mean(successes)}, std: {np.std(successes)}")
        print(f"Expected mean: {args.iterations // 2}, expected std: {(args.iterations * 0.5 * 0.5) ** 0.5}")

    elif True:  # Rough integral of bh.
        flipper = Flipper(args.delta, args.threshold)
        total = flipper.weighted_value(args.iterations)
        while True:
            # Imbalances come because a pair of flips go in a direction.
            flipper.flip_direction(up=True)
            flipper.flip_direction(up=True)
            v = flipper.weighted_value(args.iterations)
            total += v
            if v < 1e-20:
                break

        flipper = Flipper(args.delta, args.threshold)
        while True:
            # Imbalances come because a pair of flips go in a direction.
            flipper.flip_direction(up=False)
            flipper.flip_direction(up=False)
            v = flipper.weighted_value(args.iterations)
            total += v
            if v < 1e-20:
                break

        print(total)

