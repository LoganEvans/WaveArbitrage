from datetime import datetime
from multiprocessing import Pool
from pprint import pprint
import IEXTools
import argparse
import market_data_pb2
import os
import time


def debug(f):
    def spilled_milk(*args, **kwargs):
        print("> {name}({args}, {kwargs})".format(
            name=f.__name__,
            args=", ".join([str(arg) for arg in args]),
            kwargs=", ".join(
                [f"{key}={value}" for key, value in kwargs.items()])))
        res = f(*args, **kwargs)
        print(f"< {f.__name__} #=> {str(res)[:100]}")
        return res
    return spilled_milk


FPATH = os.path.split(os.path.abspath(__file__))[0]

parser = argparse.ArgumentParser(description="Parse historical data")
parser.add_argument(
        "--processed_folder", type=str,
        default=os.path.join(FPATH, "processed"),
        help="Location for processed protos.")
parser.add_argument(
        "--download_folder", type=str,
        default=os.path.join(FPATH, "IEX_data"),
        help="Location for PCAP data.")
parser.add_argument(
        "--work_claimed_folder", type=str,
        default=os.path.join(FPATH, "work_claimed"),
        help="Mutexes for claiming work.")
args = parser.parse_args()

SP500_2015 = set([
        'ABT', 'ABBV', 'ACN', 'ACE', 'ADBE', 'ADT', 'AAP', 'AES', 'AET', 'AFL',
        'AMG', 'A', 'GAS', 'APD', 'ARG', 'AKAM', 'AA', 'AGN', 'ALXN', 'ALLE',
        'ADS', 'ALL', 'ALTR', 'MO', 'AMZN', 'AEE', 'AAL', 'AEP', 'AXP', 'AIG',
        'AMT', 'AMP', 'ABC', 'AME', 'AMGN', 'APH', 'APC', 'ADI', 'AON', 'APA',
        'AIV', 'AMAT', 'ADM', 'AIZ', 'T', 'ADSK', 'ADP', 'AN', 'AZO', 'AVGO',
        'AVB', 'AVY', 'BHI', 'BLL', 'BAC', 'BK', 'BCR', 'BXLT', 'BAX', 'BBT',
        'BDX', 'BBBY', 'BRK-B', 'BBY', 'BLX', 'HRB', 'BA', 'BWA', 'BXP', 'BSK',
        'BMY', 'BRCM', 'BF-B', 'CHRW', 'CA', 'CVC', 'COG', 'CAM', 'CPB', 'COF',
        'CAH', 'HSIC', 'KMX', 'CCL', 'CAT', 'CBG', 'CBS', 'CELG', 'CNP', 'CTL',
        'CERN', 'CF', 'SCHW', 'CHK', 'CVX', 'CMG', 'CB', 'CI', 'XEC', 'CINF',
        'CTAS', 'CSCO', 'C', 'CTXS', 'CLX', 'CME', 'CMS', 'COH', 'KO', 'CCE',
        'CTSH', 'CL', 'CMCSA', 'CMA', 'CSC', 'CAG', 'COP', 'CNX', 'ED', 'STZ',
        'GLW', 'COST', 'CCI', 'CSX', 'CMI', 'CVS', 'DHI', 'DHR', 'DRI', 'DVA',
        'DE', 'DLPH', 'DAL', 'XRAY', 'DVN', 'DO', 'DTV', 'DFS', 'DISCA',
        'DISCK', 'DG', 'DLTR', 'D', 'DOV', 'DOW', 'DPS', 'DTE', 'DD', 'DUK',
        'DNB', 'ETFC', 'EMN', 'ETN', 'EBAY', 'ECL', 'EIX', 'EW', 'EA', 'EMC',
        'EMR', 'ENDP', 'ESV', 'ETR', 'EOG', 'EQT', 'EFX', 'EQIX', 'EQR', 'ESS',
        'EL', 'ES', 'EXC', 'EXPE', 'EXPD', 'ESRX', 'XOM', 'FFIV', 'FB', 'FAST',
        'FDX', 'FIS', 'FITB', 'FSLR', 'FE', 'FSIV', 'FLIR', 'FLS', 'FLR',
        'FMC', 'FTI', 'F', 'FOSL', 'BEN', 'FCX', 'FTR', 'GME', 'GPS', 'GRMN',
        'GD', 'GE', 'GGP', 'GIS', 'GM', 'GPC', 'GNW', 'GILD', 'GS', 'GT',
        'GOOGL', 'GOOG', 'GWW', 'HAL', 'HBI', 'HOG', 'HAR', 'HRS', 'HIG',
        'HAS', 'HCA', 'HCP', 'HCN', 'HP', 'HES', 'HPQ', 'HD', 'HON', 'HRL',
        'HSP', 'HST', 'HCBK', 'HUM', 'HBAN', 'ITW', 'IR', 'INTC', 'ICE', 'IBM',
        'IP', 'IPG', 'IFF', 'INTU', 'ISRG', 'IVZ', 'IRM', 'JEC', 'JBHT', 'JNJ',
        'JCI', 'JOY', 'JPM', 'JNPR', 'KSU', 'K', 'KEY', 'GMCR', 'KMB', 'KIM',
        'KMI', 'KLAC', 'KSS', 'KRFT', 'KR', 'LB', 'LLL', 'LH', 'LRCX', 'LM',
        'LEG', 'LEN', 'LVLT', 'LUK', 'LLY', 'LNC', 'LLTC', 'LMT', 'L', 'LOW',
        'LYB', 'MTB', 'MAC', 'M', 'MNK', 'MRO', 'MPC', 'MAR', 'MMC', 'MLM',
        'MAS', 'MA', 'MAT', 'MKC', 'MCD', 'MHFI', 'MCK', 'MJN', 'MMV', 'MDT',
        'MRK', 'MET', 'KORS', 'MCHP', 'MU', 'MSFT', 'MHK', 'TAP', 'MDLZ',
        'MON', 'MNST', 'MCO', 'MS', 'MOS', 'MSI', 'MUR', 'MYL', 'NDAQ', 'NOV',
        'NAVI', 'NTAP', 'NFLX', 'NWL', 'NFX', 'NEM', 'NWSA', 'NEE', 'NLSN',
        'NKE', 'NI', 'NE', 'NBL', 'JWN', 'NSC', 'NTRS', 'NOC', 'NRG', 'NUE',
        'NVDA', 'ORLY', 'OXY', 'OMC', 'OKE', 'ORCL', 'OI', 'PCAR', 'PLL', 'PH',
        'PDCO', 'PAYX', 'PNR', 'PBCT', 'POM', 'PEP', 'PKI', 'PRGO', 'PFE',
        'PCG', 'PM', 'PSX', 'PNW', 'PXD', 'PBI', 'PCL', 'PNC', 'RL', 'PPG',
        'PPL', 'PX', 'PCP', 'PCLN', 'PFG', 'PG', 'PGR', 'PLD', 'PRU', 'PEG',
        'PSA', 'PHM', 'PVH', 'QRVO', 'PWR', 'QCOM', 'DGX', 'RRC', 'RTN', 'O',
        'RHT', 'REGN', 'RF', 'RSG', 'RAI', 'RHI', 'ROK', 'COL', 'ROP', 'ROST',
        'RLC', 'R', 'CRM', 'SNDK', 'SCG', 'SLB', 'SNI', 'STX', 'SEE', 'SRE',
        'SHW', 'SIAL', 'SPG', 'SWKS', 'SLG', 'SJM', 'SNA', 'SO', 'LUV', 'SWN',
        'SE', 'STJ', 'SWK', 'SPLS', 'SBUX', 'HOT', 'STT', 'SRCL', 'SYK', 'STI',
        'SYMC', 'SYY', 'TROW', 'TGT', 'TEL', 'TE', 'TGNA', 'THC', 'TDC', 'TSO',
        'TXN', 'TXT', 'HSY', 'TRV', 'TMO', 'TIF', 'TWX', 'TWC', 'TJK', 'TMK',
        'TSS', 'TSCO', 'RIG', 'TRIP', 'FOXA', 'TSN', 'TYC', 'UA', 'UNP', 'UNH',
        'UPS', 'URI', 'UTX', 'UHS', 'UNM', 'URBN', 'VFC', 'VLO', 'VAR', 'VTR',
        'VRSN', 'VZ', 'VRTX', 'VIAB', 'V', 'VNO', 'VMC', 'WMT', 'WBA', 'DIS',
        'WM', 'WAT', 'ANTM', 'WFC', 'WDC', 'WU', 'WY', 'WHR', 'WFM', 'WMB',
        'WEC', 'WYN', 'WYNN', 'XEL', 'XRX', 'XLNX', 'XL', 'XYL', 'YHOO', 'YUM',
        'ZBH', 'ZION', 'ZTS',
    ])

class BacktestException(Exception):
    pass


prev_processed = set()
if os.path.exists(args.processed_folder):
    for f in os.listdir(args.processed_folder):
        _, date = f.split("_")
        prev_processed.add(date)


def date_to_str(date: datetime) -> str:
    return f"{date.year}{date.month:0>2}{date.day:0>2}"


class Processor:
    def filename(self, date: datetime, symbol: str):
        return os.path.join(
            args.processed_folder,
            f"{symbol}_{date_to_str(date)}")

    def process(self, date: datetime):
        if date_to_str(date) in prev_processed:
            return

        work_mutex = os.path.join(args.work_claimed_folder, date_to_str(date))
        os.open(work_mutex, os.O_CREAT | os.O_EXCL)

        keys = set(SP500_2015)
        self.processed = {key: market_data_pb2.Events() for key in keys}

        version = 1.6
        if date < datetime(2017, 8, 26):
            version = 1.5

        tries = 3
        while tries:
            raw = download_pcap(date)
            if raw is None:
                return

            try:
                p = IEXTools.Parser(raw, tops_version=version)
                break
            except FileNotFoundError:
                tries -= 1
                time.sleep(1)
        else:
            raise BacktestException(f"Exhausted tries to download pcap {date}")

        allowed = [
                IEXTools.messages.TradeReport,
                IEXTools.messages.SecurityDirective,
                #IEXTools.messages.QuoteUpdate,
                IEXTools.messages.OfficialPrice,
            ]

        while True:
            try:
                m = p.get_next_message(allowed)
            except StopIteration:
                break

            if m.symbol not in SP500_2015:
                continue

            event = self.processed[m.symbol].events.add()
            event_type = None
            if isinstance(m, IEXTools.messages.TradeReport):
                event_type = event.trade
                event_type.shares = m.size
                event_type.price = m.price_int
                # To print the timestamp:
                # print(datetime.utcfromtimestamp(
                #       event.trade.timestamp.seconds +
                #       event.trade.timestamp.nanos / 1e9))
            elif isinstance(m, IEXTools.SecurityDirective):
                event_type = event.security_directory
                event_type.round_lot = m.round_lot_size
                event_type.adjusted_poc_price = m.adjusted_poc_close
            elif isinstance(m, IEXTools.messages.QuoteUpdate):
                event_type = event.quote_update
                event_type.bid_size = m.bid_size
                event_type.bid_price = m.bid_price_int
                event_type.ask_size = m.ask_size
                event_type.ask_price = m.ask_price_int
            elif isinstance(m, IEXTools.messages.OfficialPrice):
                event_type = event.official_price
                event_type.price_type = m.price_type
                event_type.price = m.price_int
            else:
                raise BacktestException(f"Weird message: {m}")

            event_type.symbol = m.symbol
            event_type.timestamp.FromDatetime(
                    datetime.utcfromtimestamp(m.timestamp / int(1e9)))

        os.remove(raw)

        for key, proto in self.processed.items():
            with open(self.filename(date, key), "wb") as fout:
                fout.write(proto.SerializeToString())


@debug
def download_pcap(date: datetime):
    prefix = date_to_str(date)
    if prefix in prev_processed:
        return

    for fname in os.listdir(args.download_folder):
        if fname.startswith(prefix):
            return os.path.join(args.download_folder, fname)

    try:
        dl = IEXTools.DataDownloader(args.download_folder)
        return dl.download_decompressed(date, feed_type="tops")
    except IEXTools.IEXHISTExceptions.RequestsException:
        return


if __name__ == "__main__":
    for path in [args.processed_folder, args.download_folder,
                 args.work_claimed_folder]:
        if not os.path.exists(path):
            os.mkdir(path)

    if not os.path.exists(args.processed_folder):
        os.mkdir(args.processed_folder)

    p = Processor()
    for year in range(2016, 2022):
        for month in range(1, 13):
            for day in range(1, 32):
                date = datetime(2016, 1, 1)
                try:
                    date = datetime(year, month, day)
                except ValueError:
                    continue
                try:
                    p.process(date)
                except FileExistsError:
                    pass

