import datetime
import pandas as pd
from collections import defaultdict
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import matplotlib
import numpy as np
from matplotlib.ticker import FixedLocator
import matplotlib.ticker


INPUT_FILE = "data/DONE-Activity-7-4-24.csv"
OUTPUT_DATE = "2024March"

OUTPUT_PREFIX = f"data/migraines-{OUTPUT_DATE}"
OUTPUT_PREFIX_BW = f"data/migraines-byweekday-{OUTPUT_DATE}"
OUTPUT_PREFIX_BM = f"data/migraines-bymonth-{OUTPUT_DATE}"


def main():
    font = {'family' : 'sans',
            'weight' : 'normal',
            'size'   : 8}
    matplotlib.rc('font', **font)

    atdate = defaultdict(str)
    data = []
    with open(INPUT_FILE, "r", encoding="utf-8") as fh:
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
    df_out = df.sort_values(by=['date'], ascending=False)
    print(df_out)
    df_out.to_excel(f"{OUTPUT_PREFIX}.xlsx")

    make_monthly(df)
    make_weekdayly(df)

    intermediate_html = f"{OUTPUT_PREFIX}.html"
    to_html_pretty(df_out[["date", "comment"]], intermediate_html, "Migraine Journal")

    import weasyprint
    out_pdf= f"{OUTPUT_PREFIX}.pdf"
    weasyprint.HTML(intermediate_html).write_pdf(out_pdf)


def make_weekdayly(df):
    df2 = df[["weekday", "comment"]]
    df2['weekday'] = pd.Categorical(df2['weekday'], ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"])
    df2 = df2.groupby("weekday").count()
    print(df2)

    _fig, ax = plt.subplots()
    ypos = np.arange(len(df2))
    ax.barh(ypos, df2["comment"], align='center')
    ax.set_yticks(ypos, labels=df2.index)
    ax.invert_yaxis()
    ax.set_title('Number of Days with Migraine & Sumatriptan by Day of the Week')
    ax.set_aspect(3)
    plt.savefig(f"{OUTPUT_PREFIX_BW}.svg", pad_inches=0.0, dpi=300)
    #plt.show()


def make_monthly(df):
    df2 = df[["month", "comment"]].groupby("month").count()

    for year in range(2018, 2025):
        for month in range(1, 13):
            if (year == 2018 and month < 7) or (year == 2024 and month > 3):
                continue
            idx = f'{year}-{month:02}'
            if idx not in df2.index:
                df2.loc[idx] =[0]

    df2 = df2.sort_index()
    print(df2)

    color_key = {
        ("2022-03", "2022-09"): "tab:red",
        ("2022-09", "2023-02"): "tab:blue",
        ("2023-02", "2024-02"): "tab:red",
        ("2024-02", "2025-02"): "tab:blue",
    }
    colors = []
    labels = ['' ,'']
    months = 'Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec'.split()

    for idx in df2.index:
        for (d1, d2), color in color_key.items():
            if d1 <= idx < d2:
                colors.append(color)
                break
        else:
            colors.append("tab:brown")

    for month in df2.index:
        nmonth = int(month[-2:])
        if nmonth in (1, 4, 7, 10):
            prettymonth = months[nmonth-1]
            labels.append(month.split('-')[0] + '-' + prettymonth)
        else:
            pass

    _fig, ax = plt.subplots()
    plt.subplots_adjust(left=0.05, right=1.0, bottom=0.0, top=1.0)
    ax.bar(df2.index, df2['comment'], color=colors)
    ax.set_ylabel('')
    ax.set_title('Number of Days with Migraine & Sumatriptan by Month')
    ax.set_xticklabels(labels)
    ax.set_aspect(3)

    ax.xaxis.set_minor_locator(matplotlib.ticker.MultipleLocator(1.0))
    ax.xaxis.set_major_locator(matplotlib.ticker.MultipleLocator(3.0))
    ax.tick_params('both', length=6, width=1, which='major', pad=-2)
    ax.tick_params('both', length=1, width=0.5, which='minor')

    custom_lines = [Line2D([0], [0], color='tab:brown', lw=4),
                    Line2D([0], [0], color='tab:red', lw=4),
                    Line2D([0], [0], color='tab:blue', lw=4)]
    ax.legend(custom_lines, ["none", "propranolol", "topiramate"], title='Prophylaxis')
    plt.xticks(rotation=30, ha="right")

    plt.savefig(f"{OUTPUT_PREFIX_BM}.svg", pad_inches=0.0, dpi=300)
    #plt.show()



def to_html_pretty(df, filename='/tmp/out.html', title=''):
    '''
    Write an entire dataframe to an HTML file
    with nice formatting.
    Thanks to @stackoverflowuser2010 for the
    pretty printer see https://stackoverflow.com/a/47723330/362951
    '''
    ht = ''
    if title != '':
        ht += '<h2> %s </h2>\n' % title
    ht += df.to_html(classes='wide', escape=False, index=False)

    with open(filename, 'w') as f:
         f.write(HTML_TEMPLATE1 + ht + HTML_TEMPLATE2)

HTML_TEMPLATE1 = '''
<html>
<head>
<style>
  h2 {
    text-align: center;
    font-family: Helvetica, Arial, sans-serif;
  }
  table {
    margin-left: auto;
    margin-right: auto;
  }
  table, th, td {
    border: 0px solid black;
    border-collapse: collapse;
  }
  th, td {
    padding: 5px;
    text-align: left;
    font-family: Helvetica, Arial, sans-serif;
    font-size: 8pt;
  }
  table tbody tr:hover {
    background-color: #dddddd;
  }
  .wide {
    width: 100%;
  }
  body {
      margin: 0px 0px;
      padding: 0px 0px;
  }
</style>
</head>
<body>
<H4>Mikael Onsj&#246;</H4>
<H5>''' + str(datetime.date.today()) + '''</H5>
<P>
<IMG src="''' + OUTPUT_PREFIX_BM.split('/')[-1] + '''.svg"><br>
<small><i>* est. underreported by 20% from 2022 on, and 40% prior to 2022.</i><small>
<IMG src="''' + OUTPUT_PREFIX_BW.split('/')[-1] + '''.svg">
</P>
'''

HTML_TEMPLATE2 = '''
</body>
</html>
'''

if __name__ == "__main__":
    main()
