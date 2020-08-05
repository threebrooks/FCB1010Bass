cp *.service /etc/systemd/system/
sudo systemctl enable FCB1010.service
#sudo systemctl enable button_shutdown.service
sudo systemctl start FCB1010.service
#sudo systemctl start button_shutdown.service
