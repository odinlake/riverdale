import datetime
import pandas as pd


def main():
    data = []
    with open("data/DONE-Activity-11-2-24.csv", "r", encoding="utf-8") as fh:
        for line in fh.readlines():
            parts = line.strip().split(",", 6)
            ptype, _, datestr, weekday, _, count, comment = parts
            if ptype == "Migraine":
                date = datetime.datetime.strptime(datestr, '%Y%m%d').date()
                data.append([date, weekday, date.strftime("%Y"), date.strftime("%Y-%m"), comment])
    df = pd.DataFrame(data=data, columns=["date", "weekday", "year", "month", "comment"])
    print(df)
    df.sort_values("date")
    df.to_excel("data/migraines-2024Jan.xlsx")

if __name__ == "__main__":
    main()
