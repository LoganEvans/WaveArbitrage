import argparse
import sys
import collections
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('--delta', type=float, default=0.001)
parser.add_argument('--threshold', type=float, default=0.005)
parser.add_argument('--interval', type=int, default=10000)
parser.add_argument('--iterations', type=int, default=1000000)
parser.add_argument('--num_stocks', type=int, default=2)
parser.add_argument('--print_interval_status', type=bool, default=False)
parser.add_argument('--remove_common_flips', type=bool, default=False)

class Flipper:
    def __init__(self, delta, threshold, interval):
        self.delta = delta
        self.threshold = threshold
        self.interval = interval
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
        if args.remove_common_flips:
            flips = [1, 0] if np.random.binomial(1, 0.5, 1) else [0, 1]
        else:
            flips = np.random.binomial(1, 0.5, args.num_stocks)
        self.prices = [
                val * (1.0 + self.delta) if flip else val / (1.0 + self.delta)
                for flip, val in zip(flips, self.prices)]

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
        bh_values = collections.deque()
        wave_values = collections.deque()

        delta = None
        for i in range(samples):
            self.flip()
            frac = self.total_value(self.wave) / args.num_stocks
            vals = [price * size for price, size in zip(self.prices, self.wave)]

            if max(vals) / frac - 1.0 > self.threshold:
                self.rebalance()

            #if i % 100000 == 0:
            #    print(i, flipper, file=sys.stderr)

            if args.print_interval_status:
                total_bh = self.total_value(self.bh)
                total_wave = self.total_value(self.wave)

                bh_values.append(total_bh)
                wave_values.append(total_wave)

                if len(bh_values) > self.interval:
                    bh_val = bh_values.popleft()
                    wave_val = wave_values.popleft()

                    bh = bh_values[-1] / bh_val
                    wave = wave_values[-1] / wave_val

                    print(','.join([
                            '{},{},{},{},{},{},{}'.format(
                                bh, wave, wave - bh,
                                self.total_value(self.bh), self.total_value(self.wave),
                                self.g(self.bh), self.g(self.wave)),
                            ','.join([str(price) for price in self.prices]),
                            ','.join([str(total_bh / price) for price in self.prices]),
                            ','.join([str(total_wave / price) for price in self.prices])]))


if __name__ == '__main__':
    args = parser.parse_args()

    if False:
        flipper = Flipper(args.delta, args.threshold, args.interval)
        if args.print_interval_status:
            header = ','.join([
                    'bh_gain,wave_gain,diff_gain,bh_value,wave_value,g_bh,g_wave',
                    ','.join([
                        'price{}'.format(i) for i in range(args.num_stocks)]),
                    ','.join([
                        'potential_val_bh{}'.format(i) for i in range(args.num_stocks)]),
                    ','.join([
                        'potential_val_wave{}'.format(i) for i in range(args.num_stocks)])])
            print(header)

        flipper.simulate(args.iterations)

        print(flipper, file=sys.stderr)
    else:
        bh_results = []
        wave_results = []
        print(args)
        print(args.iterations)
        trials = 100
        for i in range(trials):
            flipper = Flipper(args.delta, args.threshold, 0)
            flipper.simulate(args.iterations)
            bh_results.append(flipper.g(flipper.bh))
            wave_results.append(flipper.g(flipper.wave))
            print('progress: {} / {} -- {}(min: {}, max:{}, recent: {}), {}\n'.format(i, trials, np.mean(bh_results), min(bh_results), max(bh_results), bh_results[-1], np.mean(wave_results)), end='')
            sys.stdout.flush()
        print()
        print(np.mean(bh_results), np.mean(wave_results))

