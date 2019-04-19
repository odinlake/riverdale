import influxdb
import boto3
from datetime import datetime, timedelta
import json
import zlib
import sys
import socket
import re


CLIENT = {}


def influxdb_client(task=None):
    global CLIENT
    hname = socket.gethostname()
    if hname == "ip-172-31-12-210":
        host = "localhost"
    else:
        host = "192.168.1.15"
    task = task or (next(iter(CLIENT)) if CLIENT else "export")
    if task not in CLIENT:
        dbn = "domoticz" if task == "export" else "domoticz_hist" if task == "import" else None
        client = CLIENT[task] = influxdb.InfluxDBClient(host, 8086, '', '', dbn)
        dbns = [x["name"] for x in client.get_list_database()]
        if dbn not in dbns:
            client.create_database(dbn)
    return CLIENT[task]


def influxdb_query(q, task=None):
    print(q)
    return influxdb_client(task).query(q)


def get_s3bucket():
    s3 = boto3.resource('s3')
    bucket = s3.Bucket('mo-riverdale')
    return bucket


def get_data_influxdb(day):
    """
    Retrieve data for date, in 5 min aggregates, from local influxdb.
    """
    d1s = day.isoformat() + " 00:00:00"
    d2s = (day + timedelta(days=1)).isoformat() + " 00:00:00"
    cols = [x['name'] for x in influxdb_client().get_list_measurements()]
    colsq = ", ".join('"{}"'.format(x) for x in cols)
    data = influxdb_query("""
        SELECT mean(value) as mean, max(value) as max 
        FROM {} 
        WHERE time >= '{}' AND time < '{}'
        GROUP BY "name", time(5m) fill(none)
    """.format(colsq, d1s, d2s))
    if not data:
        return []
    rets = []
    for item in data.raw["series"]:
        vidx = 2 if item["name"] == "Sound-Level" else 1
        values = [(v[0], v[vidx]) for v in item["values"]]
        row = (item["name"], item["tags"]["name"], values)
        if values:
            print(row[:2]+(len(values), values[0]))
            rets.append(row)
    return rets


def export_s3():
    """
    Read data from local influxdb and dump it in Amzon S3 bucket.
    """
    bucket = get_s3bucket()
    objs = [o.key for o in bucket.objects.all()]
    day = datetime.utcnow().date()
    keyexistscount = 0
    print("S3 has objects: {}... ({})".format(objs[:5], len(objs)))

    # always overwrite two dates as the most recent dates could be incomplete
    while keyexistscount < 2:
        print("EXPORTING DATE: " + day.isoformat())
        key = "sensordata.{}.json.gz".format(day.isoformat())
        if key in objs:
            keyexistscount += 1
        data = get_data_influxdb(day)
        day -= timedelta(days = 1)
        if data:
            jdata = json.dumps(data)
            zjdata = zlib.compress(jdata.encode("utf-8"))
            obj = bucket.Object(key=key)
            obj.put(Body=zjdata)
            print("Wrote: " + key)
        else:
            break


def get_latest(measurements):
    """
    Return the last point for each measurement and tag.
    """
    colsq = ", ".join('"{}"'.format(m) for m in measurements)
    data = influxdb_query("""
        SELECT value FROM {}
        GROUP BY * ORDER BY DESC LIMIT 1
    """.format(colsq))
    dates = []
    for seriesd in data.raw["series"]:
        dstr = seriesd["values"][0][0][:10]
        dt = datetime.strptime(dstr, '%Y-%m-%d').date()
        dates.append(dt)
    dt = max(dates)
    if dt -timedelta(days=1) in dates:
        dt -= timedelta(days=1)
    return dt


def import_s3():
    """
    Read data from Amazon S3 bucket and import to local network influxdb.
    """
    client = influxdb_client("import")
    measurements = set(x["name"] for x in client.get_list_measurements())
    dt = get_latest(measurements)
    bucket = get_s3bucket()
    for obj in bucket.objects.all():
        
        print("IMPORTING: {}".format(obj.key))
        zjdata = obj.get()["Body"].read()
        jdata = zlib.decompress(zjdata)
        data = json.loads(jdata)
        for series, tag, values in data:
            print((series, tag, len(values)))
            client.write_points([{
                "measurement": series,
                "tags": {"name": tag},
                "fields": {"value": float(v[1])},
                "time": v[0],
            } for v in values])


def main():
    influxdb_client("import")
    return get_latest(["Temperature", "Humidity"])

    if "--export" in sys.argv:
        export_s3()
    elif "--import" in sys.argv:
        import_s3()
    else:
        print("Specify --import or --export.")



