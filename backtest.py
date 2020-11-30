from datetime import datetime
import IEXTools
import os
import market_data_pb2
from pprint import pprint

FPATH = os.path.split(os.path.abspath(__file__))[0]

SP500_2015 = set([
        'ABT', 'ABBV', 'ACN', 'ACE', 'ADBE', 'ADT', 'AAP', 'AES', 'AET',
        'AFL', 'AMG', 'A', 'GAS', 'APD', 'ARG', 'AKAM', 'AA', 'AGN', 'ALXN', 'ALLE',
        'ADS', 'ALL', 'ALTR', 'MO', 'AMZN', 'AEE', 'AAL', 'AEP', 'AXP', 'AIG', 'AMT',
        'AMP', 'ABC', 'AME', 'AMGN', 'APH', 'APC', 'ADI', 'AON', 'APA', 'AIV', 'AMAT',
        'ADM', 'AIZ', 'T', 'ADSK', 'ADP', 'AN', 'AZO', 'AVGO', 'AVB', 'AVY', 'BHI',
        'BLL', 'BAC', 'BK', 'BCR', 'BXLT', 'BAX', 'BBT', 'BDX', 'BBBY', 'BRK-B', 'BBY',
        'BLX', 'HRB', 'BA', 'BWA', 'BXP', 'BSK', 'BMY', 'BRCM', 'BF-B', 'CHRW', 'CA',
        'CVC', 'COG', 'CAM', 'CPB', 'COF', 'CAH', 'HSIC', 'KMX', 'CCL', 'CAT', 'CBG',
        'CBS', 'CELG', 'CNP', 'CTL', 'CERN', 'CF', 'SCHW', 'CHK', 'CVX', 'CMG', 'CB',
        'CI', 'XEC', 'CINF', 'CTAS', 'CSCO', 'C', 'CTXS', 'CLX', 'CME', 'CMS', 'COH',
        'KO', 'CCE', 'CTSH', 'CL', 'CMCSA', 'CMA', 'CSC', 'CAG', 'COP', 'CNX', 'ED',
        'STZ', 'GLW', 'COST', 'CCI', 'CSX', 'CMI', 'CVS', 'DHI', 'DHR', 'DRI', 'DVA',
        'DE', 'DLPH', 'DAL', 'XRAY', 'DVN', 'DO', 'DTV', 'DFS', 'DISCA', 'DISCK', 'DG',
        'DLTR', 'D', 'DOV', 'DOW', 'DPS', 'DTE', 'DD', 'DUK', 'DNB', 'ETFC', 'EMN',
        'ETN', 'EBAY', 'ECL', 'EIX', 'EW', 'EA', 'EMC', 'EMR', 'ENDP', 'ESV', 'ETR',
        'EOG', 'EQT', 'EFX', 'EQIX', 'EQR', 'ESS', 'EL', 'ES', 'EXC', 'EXPE', 'EXPD',
        'ESRX', 'XOM', 'FFIV', 'FB', 'FAST', 'FDX', 'FIS', 'FITB', 'FSLR', 'FE',
        'FSIV', 'FLIR', 'FLS', 'FLR', 'FMC', 'FTI', 'F', 'FOSL', 'BEN', 'FCX', 'FTR',
        'GME', 'GPS', 'GRMN', 'GD', 'GE', 'GGP', 'GIS', 'GM', 'GPC', 'GNW', 'GILD',
        'GS', 'GT', 'GOOGL', 'GOOG', 'GWW', 'HAL', 'HBI', 'HOG', 'HAR', 'HRS', 'HIG',
        'HAS', 'HCA', 'HCP', 'HCN', 'HP', 'HES', 'HPQ', 'HD', 'HON', 'HRL', 'HSP',
        'HST', 'HCBK', 'HUM', 'HBAN', 'ITW', 'IR', 'INTC', 'ICE', 'IBM', 'IP', 'IPG',
        'IFF', 'INTU', 'ISRG', 'IVZ', 'IRM', 'JEC', 'JBHT', 'JNJ', 'JCI', 'JOY', 'JPM',
        'JNPR', 'KSU', 'K', 'KEY', 'GMCR', 'KMB', 'KIM', 'KMI', 'KLAC', 'KSS', 'KRFT',
        'KR', 'LB', 'LLL', 'LH', 'LRCX', 'LM', 'LEG', 'LEN', 'LVLT', 'LUK', 'LLY',
        'LNC', 'LLTC', 'LMT', 'L', 'LOW', 'LYB', 'MTB', 'MAC', 'M', 'MNK', 'MRO',
        'MPC', 'MAR', 'MMC', 'MLM', 'MAS', 'MA', 'MAT', 'MKC', 'MCD', 'MHFI', 'MCK',
        'MJN', 'MMV', 'MDT', 'MRK', 'MET', 'KORS', 'MCHP', 'MU', 'MSFT', 'MHK', 'TAP',
        'MDLZ', 'MON', 'MNST', 'MCO', 'MS', 'MOS', 'MSI', 'MUR', 'MYL', 'NDAQ', 'NOV',
        'NAVI', 'NTAP', 'NFLX', 'NWL', 'NFX', 'NEM', 'NWSA', 'NEE', 'NLSN', 'NKE',
        'NI', 'NE', 'NBL', 'JWN', 'NSC', 'NTRS', 'NOC', 'NRG', 'NUE', 'NVDA', 'ORLY',
        'OXY', 'OMC', 'OKE', 'ORCL', 'OI', 'PCAR', 'PLL', 'PH', 'PDCO', 'PAYX', 'PNR',
        'PBCT', 'POM', 'PEP', 'PKI', 'PRGO', 'PFE', 'PCG', 'PM', 'PSX', 'PNW', 'PXD',
        'PBI', 'PCL', 'PNC', 'RL', 'PPG', 'PPL', 'PX', 'PCP', 'PCLN', 'PFG', 'PG',
        'PGR', 'PLD', 'PRU', 'PEG', 'PSA', 'PHM', 'PVH', 'QRVO', 'PWR', 'QCOM', 'DGX',
        'RRC', 'RTN', 'O', 'RHT', 'REGN', 'RF', 'RSG', 'RAI', 'RHI', 'ROK', 'COL',
        'ROP', 'ROST', 'RLC', 'R', 'CRM', 'SNDK', 'SCG', 'SLB', 'SNI', 'STX', 'SEE',
        'SRE', 'SHW', 'SIAL', 'SPG', 'SWKS', 'SLG', 'SJM', 'SNA', 'SO', 'LUV', 'SWN',
        'SE', 'STJ', 'SWK', 'SPLS', 'SBUX', 'HOT', 'STT', 'SRCL', 'SYK', 'STI', 'SYMC',
        'SYY', 'TROW', 'TGT', 'TEL', 'TE', 'TGNA', 'THC', 'TDC', 'TSO', 'TXN', 'TXT',
        'HSY', 'TRV', 'TMO', 'TIF', 'TWX', 'TWC', 'TJK', 'TMK', 'TSS', 'TSCO', 'RIG',
        'TRIP', 'FOXA', 'TSN', 'TYC', 'UA', 'UNP', 'UNH', 'UPS', 'URI', 'UTX', 'UHS',
        'UNM', 'URBN', 'VFC', 'VLO', 'VAR', 'VTR', 'VRSN', 'VZ', 'VRTX', 'VIAB', 'V',
        'VNO', 'VMC', 'WMT', 'WBA', 'DIS', 'WM', 'WAT', 'ANTM', 'WFC', 'WDC', 'WU',
        'WY', 'WHR', 'WFM', 'WMB', 'WEC', 'WYN', 'WYNN', 'XEL', 'XRX', 'XLNX', 'XL',
        'XYL', 'YHOO', 'YUM', 'ZBH', 'ZION', 'ZTS',
    ])

class BacktestException(Exception):
    pass


class Processor:
    def __init__(self, processed_folder=os.path.join(FPATH, "processed")):
        if not os.path.exists(processed_folder):
            os.mkdir(processed_folder)
        self.processed_folder = processed_folder

    def filename(self, date: datetime, symbol: str):
        return os.path.join(
            self.processed_folder,
            f"{symbol}_{date.year}{date.month:2>0}{date.day:0>2}")

    def process(self, date: datetime):
        keys = set(SP500_2015)
        to_remove = set()
        prev_processed = os.listdir(self.processed_folder)
        for key in keys:
            if self.filename(date, key) in prev_processed:
                prev_processed.add(key)
        keys.difference_update(to_remove)
        if not keys:
            return

        self.processed = {key: market_data_pb2.Events() for key in keys}

        dl = Downloader()
        raw = dl.download(date)
        if raw is None:
            raise BacktestException(f"No data for date '{date}'")
        p = IEXTools.Parser(raw)
        allowed = [IEXTools.messages.TradeReport, IEXTools.messages.SecurityDirective]

        m = True
        i = 0
        while m:
            m = p.get_next_message(allowed)
            if m.symbol not in SP500_2015:
                continue

            event = self.processed[m.symbol].events.add()
            if isinstance(m, IEXTools.SecurityDirective):
                event.security_directory.symbol = m.symbol
                event.security_directory.timestamp.FromDatetime(datetime.utcfromtimestamp(m.timestamp / int(1e9)))
                event.security_directory.round_lot = m.round_lot_size
                event.security_directory.adjusted_poc_price = m.adjusted_poc_close
            else:
                event.trade.symbol = m.symbol
                event.trade.timestamp.FromDatetime(datetime.utcfromtimestamp(m.timestamp / int(1e9)))
                event.trade.shares = m.size
                event.trade.price = m.price_int
                # To print the timestamp:
                # print(datetime.utcfromtimestamp(event.trade.timestamp.seconds + event.trade.timestamp.nanos / 1e9))
            i += 1
            if i % 50 == 0:
                break
                print(i, m)

        # TODO: For each key in self.processed, save the Events to a file.
        pprint(self.processed)


class Downloader:
    def __init__(self, tops_folder: str=os.path.join(FPATH, "IEX_data")):
        self.tops_folder = tops_folder

    def download(self, date: datetime):
        prefix = f"{date.year}{date.month:0>2}{date.day:0>2}"
        for fname in os.listdir(self.tops_folder):
            if fname.startswith(prefix):
                return os.path.join(self.tops_folder, fname)

        dl = IEXTools.DataDownloader(self.tops_folder)
        return dl.download_decompressed(date, feed_type="tops")


if __name__ == "__main__":
    d = Downloader()
    #print(d.download_decompressed(datetime(2018, 7, 13), feed_type="tops"))
    date = datetime(2018, 7, 13)
    d.download(date)

    p = Processor()
    p.process(date)
