import influxdb
import boto3
from datetime import datetime, timedelta
import json
import zlib
import sys


CLIENT = {}


def influxdb_client(task=None):
    global CLIENT
    task = task or (next(CLIENT) if CLIENT else "export")
    if task not in CLIENT:
        dbn = "domoticz" if task == "export" else "domoticz_hist" if task == "import" else None
        client = CLIENT[task] = influxdb.InfluxDBClient('192.168.1.15', 8086, '', '', dbn)
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
        SELECT mean(value) as mean 
        FROM {} 
        WHERE time >= '{}' AND time < '{}'
        GROUP BY "name", time(5m) fill(none)
    """.format(colsq, d1s, d2s))
    if not data:
        return []
    rets = []
    for item in data.raw["series"]:
        row = (item["name"], item["tags"]["name"], item["values"])
        if row[-1]:
            print(row[:2]+(len(row[-1]),))
            rets.append(row)
    return rets


def export_s3():
    """
    Read data from local influxdb and dump it in Amzon S3 bucket.
    """
    bucket = get_s3bucket()
    objs = [o.key for o in bucket.objects.all()]
    day = datetime.utcnow().date()
    keyexists = False
    print("S3 has objects: {}... ({})".format(objs[:5], len(objs)))

    # always overwrite one date as the most recent date could be incomplete
    while not keyexists:
        day -= timedelta(days = 1)
        print("EXPORTING DATE: " + day.isoformat())
        key = "sensordata.{}.json.gz".format(day.isoformat())
        keyexists = key in objs
        data = get_data_influxdb(day)
        if data:
            jdata = json.dumps(data)
            zjdata = zlib.compress(jdata.encode("utf-8"))
            obj = bucket.Object(key=key)
            obj.put(Body=zjdata)
            print("Wrote: " + key)
        else:
            break


def import_s3():
    """
    Read data from Amazon S3 bucket and import to local network influxdb.
    """
    client = influxdb_client("import")
    measurements = set(x["name"] for x in client.get_list_measurements())
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
    if "--export" in sys.argv:
        export_s3()
    elif "--import" in sys.argv:
        import_s3()
    else:
        print("Specify --import or --export.")



