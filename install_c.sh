#!/bin/bash
echo "Installing OVPN Client (C version)..."

sudo cp ovpn-client /usr/local/bin/
sudo chmod +x /usr/local/bin/ovpn-client

mkdir -p ~/.local/share/applications
cp ovpn-client.desktop ~/.local/share/applications/

update-desktop-database ~/.local/share/applications/ || true

echo "Installation complete!"
echo "Run 'ovpn-client' to start the application."
