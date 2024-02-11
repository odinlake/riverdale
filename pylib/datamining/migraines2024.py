import datetime
import pandas as pd
from collections import defaultdict

def main():
    atdate = defaultdict(str)
    data = []
    with open("data/DONE-Activity-11-2-24.csv", "r", encoding="utf-8") as fh:
        for line in fh.readlines():
            parts = line.strip().split(",", 6)
            ptype, _, datestr, weekday, _, count, comment = parts
            if ptype == "Migraine":
                date = datetime.datetime.strptime(datestr, '%Y%m%d').date()
                atdate[date] += comment

    for date, comment in atdate.items():
        data.append([
            date, date.strftime("%a"), date.strftime("%Y"), date.strftime("%Y-%m"), comment
        ])
    df = pd.DataFrame(data=data, columns=["date", "weekday", "year", "month", "comment"])
    df = df.set_index("date")
    df = df.sort_index()
    print(df)
    df.to_excel("data/migraines-2024Jan.xlsx")

if __name__ == "__main__":
    main()
