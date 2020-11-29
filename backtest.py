from datetime import datetime
import IEXTools
import os


class Process:
    def __init__(self, date: datetime, symbol: str):
        pass


class Downloader:
    def __init__(self, folder: str=os.path.join(os.path.split(os.path.abspath(__file__))[0],
                                                "IEX_data")):
        self.folder = folder

    def download(self, date: datetime):
        prefix = f"{date.year}{date.month:0>2}{date.day:0>2}"
        for fname in os.listdir(self.folder):
            if fname.startswith(prefix):
                return

        dl = IEXTools.DataDownloader(self.folder)
        dl.download_decompressed(date, feed_type="tops")


if __name__ == "__main__":
    d = Downloader()
    #print(d.download_decompressed(datetime(2018, 7, 13), feed_type="tops"))
    print(d.download(datetime(2018, 7, 16)))
