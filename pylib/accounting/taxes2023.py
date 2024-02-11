import pandas as pd
import datetime
from collections import OrderedDict



def iter_sells(cost_bases, amount):
    while amount > 0 and cost_bases:
        tgt = cost_bases[-1]
        if amount <= tgt["amount"]:
            yield [amount, tgt["date"], 0]
            tgt["amount"] -= amount
            amount = 0
        else:
            yield [tgt["amount"], tgt["date"], 0]
            amount -= tgt["amount"]
            tgt["amount"] = 0
        if tgt["amount"] == 0:
            cost_bases.pop()
    if amount:
        yield [amount, None, amount]


def main():
    swpath = "/mnt/e/My Drive/_records/financial/mikael-swedbank-funds-history-01Dec23.txt"
    outpath = "/mnt/e/My Drive/_records/financial/mikael-swedbank-funds-history-01Dec23.xlsx"
    types = {
        'Ingående balans': "buy",
        'Köp': "buy",
        'Inlösen': "sell",
        'Utdelning (återinvestering)': "other",
        'Flytt till': "buy",
        'Flytt från': "sell",
        'Inlösen fondbyte': "sell",
        'Köp fondbyte': "buy",
        'Kompensationsköp': "buy",
    }
    tax_year = pd.Timestamp(datetime.date.today().year - 5, 4, 6)

    with open(swpath, "r", encoding="utf-8") as fh:
        data=[x.strip() for x in fh.readlines() if x]
        data = [x for x in data if x]

    data = [data[i:i+7] for i in range(0, len(data)-1, 7)]
    for row in data:
        row[6] = float(row[6].replace(" ", "").replace(",", "."))
    df = pd.DataFrame(data, columns=["name", "j1", "date", "j2", "type", "j3", "amount"]).drop(columns=["j1", "j2", "j3"]).sort_values(["name", "date"])

    prevname = ""
    acc_cost = 0
    cost_bases = []
    data = []

    for ii, row in df.iterrows():
        print(row)
        name = row["name"]
        direction = types[row["type"]]
        amount = row["amount"]
        profit = 0

        if name != prevname:
            prevname = name
            acc_cost = 0
            cost_bases = []

        if direction == "buy":
            acc_cost += amount
            cost_bases.append(row.copy())
            data.append(list(row) + [direction, amount, acc_cost, 0, None])

        if direction == "sell":
            for delta_amount, cost_date, profit in iter_sells(cost_bases, amount):
                if profit == 0:
                    acc_cost -= delta_amount
                data.append(list(row) + [direction, delta_amount, acc_cost, profit, cost_date])


    df = pd.DataFrame(
        data,
        columns=list(df.columns) + ["direction", "part", "cost base", "profit", "cost date"]
    ).sort_values(["name", "date"])
    #df[["direction", "cost base", "profit", "cost date"]] = data
    df['date'] = pd.to_datetime(df['date'])
    dfs = OrderedDict([('ALL', df)])

    print(df)

    last_date = max(df["date"])
    while tax_year < last_date:
        next_tax_year = tax_year + pd.offsets.DateOffset(years=1)
        mask = (df['date'] >= tax_year) & (df['date'] < next_tax_year)
        dfs[f"{tax_year.date()} to {next_tax_year.date()}"] = df.loc[mask]
        tax_year = next_tax_year

    #df.to_excel(outpath, "2023")
    to_excel(outpath, dfs)

    print("Done.")


def to_excel(filename, dfs):
    writer = pd.ExcelWriter(
        filename,
        engine='xlsxwriter',
        datetime_format="yyyy-mm-dd",
    )
    workbook  = writer.book
    float_format = workbook.add_format({
        "num_format": "#,##0.00",
    })
    date_format = workbook.add_format({
        "num_format": "yyyy-mm-dd",
    }) # busted

    for sheetname, dff in dfs.items():
        df = dff.copy()
        if sheetname.lower() == "all":
            df = df.sort_values(["name", "date"])
        else:
            df = df.sort_values(["date", "name"])
        df.to_excel(writer, sheet_name=sheetname, index=False)
        worksheet = writer.sheets[sheetname]
        for idx, col in enumerate(df):
            series = df[col]
            max_len = max((
                series.astype(str).map(len).max(),
                len(str(series.name))
            ))
            if idx >= 5:
                worksheet.set_column(idx, idx, max_len+3, float_format)
            else:
                worksheet.set_column(idx, idx, max_len+3)

    writer.close()
    print(f"wrote {filename}")


if __name__ == "__main__":
    main()
