import smtplib
import urllib2
from email import encoders
from email.mime.base import MIMEBase
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from email.mime.application import MIMEApplication

from credentials import smtp


IMAGES = {
    "temperature": "http://192.168.1.15:3000/render/d-solo/9z4E5IRRk/riverdale?orgId=1&panelId=4&from=1555655107325&to=1555741507325&width=600&height=300&tz=UTC%2B01%3A00",
}


def send_mail(receivers):
    receivers = receivers if isinstance(receivers, list) else [receivers]

    message = MIMEMultipart("mixed")
    message["Subject"] = "multipart test"
    message["From"] = smtp.sender
    message["To"] = "; ".join(receivers)
    #part1 = MIMEText("""Test message\n""", "plain")
    #message.attach(part)
    part = MIMEText("""<html><body><p>test message<image src="temperature.png" width="100px" alt="here!"> done.</p></body></html>\n""", "html")
    message.attach(part)

    for imagename, url in IMAGES.items():
        data = urllib2.urlopen(url).read()
        with open("C:\\temp\\riverdale.png", "wb+") as fh:
            fh.write(data)

        part = MIMEBase("image", 'png;\n name="test.png"')
        part.set_payload(data)
        part.add_header(
            "Content-Disposition",
            'attachment;\n filename="{}.png"'.format(imagename),
        )
        encoders.encode_base64(part)
        #part = MIMEApplication(data, Name="{}.png".format(imagename))
        #part['Content-Disposition'] = 'attachment; filename="{}.png"'.format(imagename)
        message.attach(part)

    server = smtplib.SMTP(host=smtp.host, port=smtp.port)
    server.starttls()
    server.login(smtp.sender, smtp.password)
    for receiver in receivers:
        server.sendmail(smtp.sender, receiver, message.as_string())


def main():
    send_mail("mikael@odinlake.net")



