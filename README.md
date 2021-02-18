## Wave Arbitrage

Wave arbitrage is a trading algorithm that takes advantage of waves in prices.
The basic idea is to continuously rebalance a portfolio to a 50-50 ratio. This
algorithm has a higher expected value than a buy-and-hold strategy given that
the market is a "fair game", prices don't go to zero, and market prices
fluctuate. This algorithm is a disproof of the efficient market hypothesis.

## Paper

The current version of the paper is at
[paper/paper.pdf](https://github.com/LoganEvans/WaveArbitrage/blob/master/paper/paper.pdf).

## Environment

### Dependencies

* `bazel`
* `clang`
* `python3`
* Probably others.

### Setup

```
  git clone https://github.com/LoganEvans/WaveArbitrage.git
  git submodule update --init --recursive
```

## Collecting data

The `scraper.py` script fetches data from IEX and converts it into protobufs
It stores data in `$HOMEDIR/iex_data`. It also takes a long time. In order to
speed things up, you can use script-level parallelism. I'm not proud of how
this works -- well, maybe I am a little.

```
  bazel build :scraper
  for _ in `seq 10`; do bazel-bin/scraper & done
```

## Running the back test

This takes quite a while, although not as long as collecting the data.

```
  bazel run -c opt :backtest
```

## Rendering the histogram

The `backtest` binary prints out `json` representations of histograms. The
`show_histogram.py` script can render multiple histograms together as long as
they are all piped through `stdin`. As an example:

```
  lib/DynamicHistogram/scripts/show_histogram.py <<< `cat results.txt`
```

## Other simulations

The script `flip.py` and the code in `simulate.cpp` are simulations that run
wave arbitrage and buy-and-hold against some basic price models. `flip.py`
appears to be using a submartingale. `simulate.cpp` is using a martingale.

```
  python3 flip.py --help
  bazel run -c opt :simulate
```

