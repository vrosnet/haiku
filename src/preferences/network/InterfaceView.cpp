/*
 * Copyright 2004-2015 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, <axeld@pinc-software.de>
 *		Alexander von Gluck, kallisti5@unixzen.com
 *		John Scipione, jscipione@gmail.com
 */


#include "InterfaceView.h"

#include <set>

#include <net/if_media.h>

#include <Button.h>
#include <Catalog.h>
#include <ControlLook.h>
#include <LayoutBuilder.h>
#include <NetworkAddress.h>
#include <NetworkDevice.h>
#include <StringForSize.h>
#include <StringView.h>
#include <TextControl.h>

#include "MediaTypes.h"
#include "WirelessNetworkMenuItem.h"


static const uint32 kMsgInterfaceToggle = 'onof';
static const uint32 kMsgInterfaceRenegotiate = 'redo';
static const uint32 kMsgJoinNetwork = 'join';


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "IntefaceView"


// #pragma mark - InterfaceView


InterfaceView::InterfaceView()
	:
	BGroupView(B_VERTICAL),
	fPulseCount(0)
{
	SetFlags(Flags() | B_PULSE_NEEDED);

	// TODO: Small graph of throughput?

	float minimumWidth = be_control_look->DefaultItemSpacing() * 16;

	BStringView* statusLabel = new BStringView("status label",
		B_TRANSLATE("Status:"));
	statusLabel->SetAlignment(B_ALIGN_RIGHT);
	fStatusField = new BStringView("status field", "");
	fStatusField->SetExplicitMinSize(BSize(minimumWidth, B_SIZE_UNSET));
	BStringView* macAddressLabel = new BStringView("mac address label",
		B_TRANSLATE("MAC address:"));
	macAddressLabel->SetAlignment(B_ALIGN_RIGHT);
	fMacAddressField = new BStringView("mac address field", "");
	fMacAddressField->SetExplicitMinSize(BSize(minimumWidth, B_SIZE_UNSET));
	BStringView* linkSpeedLabel = new BStringView("link speed label",
		B_TRANSLATE("Link speed:"));
	linkSpeedLabel->SetAlignment(B_ALIGN_RIGHT);
	fLinkSpeedField = new BStringView("link speed field", "");
	fLinkSpeedField->SetExplicitMinSize(BSize(minimumWidth, B_SIZE_UNSET));

	// TODO: These metrics may be better in a BScrollView?
	BStringView* linkTxLabel = new BStringView("tx label",
		B_TRANSLATE("Sent:"));
	linkTxLabel->SetAlignment(B_ALIGN_RIGHT);
	fLinkTxField = new BStringView("tx field", "");
	fLinkTxField ->SetExplicitMinSize(BSize(minimumWidth, B_SIZE_UNSET));
	BStringView* linkRxLabel = new BStringView("rx label",
		B_TRANSLATE("Received:"));
	linkRxLabel->SetAlignment(B_ALIGN_RIGHT);
	fLinkRxField = new BStringView("rx field", "");
	fLinkRxField ->SetExplicitMinSize(BSize(minimumWidth, B_SIZE_UNSET));

	fNetworkMenuField = new BMenuField(B_TRANSLATE("Network:"), new BMenu(
		B_TRANSLATE("Choose automatically")));
	fNetworkMenuField->SetAlignment(B_ALIGN_RIGHT);
	fNetworkMenuField->Menu()->SetLabelFromMarked(true);

	// Construct the BButtons
	fToggleButton = new BButton("onoff", B_TRANSLATE("Disable"),
		new BMessage(kMsgInterfaceToggle));

	fRenegotiateButton = new BButton("heal", B_TRANSLATE("Renegotiate"),
		new BMessage(kMsgInterfaceRenegotiate));
	fRenegotiateButton->SetEnabled(false);

	BLayoutBuilder::Group<>(this)
		.AddGrid()
			.Add(statusLabel, 0, 0)
			.Add(fStatusField, 1, 0)
			.Add(fNetworkMenuField->CreateLabelLayoutItem(), 0, 1)
			.Add(fNetworkMenuField->CreateMenuBarLayoutItem(), 1, 1)
			.Add(macAddressLabel, 0, 2)
			.Add(fMacAddressField, 1, 2)
			.Add(linkSpeedLabel, 0, 3)
			.Add(fLinkSpeedField, 1, 3)
			.Add(linkTxLabel, 0, 4)
			.Add(fLinkTxField, 1, 4)
			.Add(linkRxLabel, 0, 5)
			.Add(fLinkRxField, 1, 5)
		.End()
		.AddGlue()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fToggleButton)
			.Add(fRenegotiateButton)
		.End();
}


InterfaceView::~InterfaceView()
{
}


void
InterfaceView::SetTo(const char* name)
{
	fInterface.SetTo(name);
}


void
InterfaceView::AttachedToWindow()
{
	_Update();
		// Populate the fields

	fToggleButton->SetTarget(this);
	fRenegotiateButton->SetTarget(this);
}


void
InterfaceView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgJoinNetwork:
		{
			const char* name;
			BNetworkAddress address;
			if (message->FindString("name", &name) == B_OK
				&& message->FindFlat("address", &address) == B_OK) {
				BNetworkDevice device(fInterface.Name());
				status_t status = device.JoinNetwork(address);
				if (status != B_OK) {
					// This does not really matter, as it's stored this way,
					// anyway.
				}
				// TODO: store value
			}
			break;
		}
		case kMsgInterfaceToggle:
		{
			// Disable/enable interface
			uint32 flags = fInterface.Flags();
			if ((flags & IFF_UP) != 0)
				flags &= ~IFF_UP;
			else
				flags |= IFF_UP;

			if (fInterface.SetFlags(flags) == B_OK)
				_Update();
			break;
		}

		case kMsgInterfaceRenegotiate:
		{
			// TODO: renegotiate addresses
			break;
		}

		default:
			BView::MessageReceived(message);
	}
}


void
InterfaceView::Pulse()
{
	// Update the wireless network menu every 5 seconds
	_Update((fPulseCount++ % 5) == 0);
}


/*!	Populate fields with current settings.
*/
status_t
InterfaceView::_Update(bool updateWirelessNetworks)
{
	BNetworkDevice device(fInterface.Name());
	bool isWireless = device.IsWireless();
	bool disabled = (fInterface.Flags() & IFF_UP) == 0;

	if (fInterface.HasLink()) {
		if (isWireless) {
			// TODO!
			BString network = "---";
			network.Prepend(" (");
			network.Prepend(B_TRANSLATE("connected"));
			network.Append(")");
			fStatusField->SetText(network.String());
		} else {
			fStatusField->SetText(B_TRANSLATE("connected"));
		}
	} else
		fStatusField->SetText(B_TRANSLATE("disconnected"));

	BNetworkAddress hardwareAddress;
	if (device.GetHardwareAddress(hardwareAddress) == B_OK)
		fMacAddressField->SetText(hardwareAddress.ToString());
	else
		fMacAddressField->SetText("-");

	int media = fInterface.Media();
	if ((media & IFM_ACTIVE) != 0)
		fLinkSpeedField->SetText(media_type_to_string(media));
	else
		fLinkSpeedField->SetText("-");

	// Update Link stats
	ifreq_stats stats;
	if (fInterface.GetStats(stats) == B_OK) {
		char buffer[100];

		string_for_size(stats.send.bytes, buffer, sizeof(buffer));
		fLinkTxField->SetText(buffer);

		string_for_size(stats.receive.bytes, buffer, sizeof(buffer));
		fLinkRxField->SetText(buffer);
	}

	// TODO: move the wireless info to a separate tab. We should have a
	// BListView of available networks, rather than a menu, to make them more
	// readable and easier to browse and select.
	if (fNetworkMenuField->IsHidden(fNetworkMenuField) && isWireless)
		fNetworkMenuField->Show();
	else if (!fNetworkMenuField->IsHidden(fNetworkMenuField) && !isWireless)
		fNetworkMenuField->Hide();

	if (isWireless && updateWirelessNetworks) {
		// Rebuild network menu
		BMenu* menu = fNetworkMenuField->Menu();
		menu->RemoveItems(0, menu->CountItems(), true);

		std::set<BNetworkAddress> associated;
		BNetworkAddress address;
		uint32 cookie = 0;
		while (device.GetNextAssociatedNetwork(cookie, address) == B_OK)
			associated.insert(address);

		wireless_network network;
		int32 count = 0;
		cookie = 0;
		while (device.GetNextNetwork(cookie, network) == B_OK) {
			BMessage* message = new BMessage(kMsgJoinNetwork);

			message->AddString("device", fInterface.Name());
			message->AddString("name", network.name);
			message->AddFlat("address", &network.address);

			BMenuItem* item = new WirelessNetworkMenuItem(network.name,
				network.signal_strength,
				network.authentication_mode, message);
			if (associated.find(network.address) != associated.end())
				item->SetMarked(true);
			menu->AddItem(item);

			count++;
		}
		if (count == 0) {
			BMenuItem* item = new BMenuItem(
				B_TRANSLATE("<no wireless networks found>"), NULL);
			item->SetEnabled(false);
			menu->AddItem(item);
		} else {
			BMenuItem* item = new BMenuItem(
				B_TRANSLATE("Choose automatically"), NULL);
			if (menu->FindMarked() == NULL)
				item->SetMarked(true);
			menu->AddItem(item, 0);
			menu->AddItem(new BSeparatorItem(), 1);
		}
		menu->SetTargetForItems(this);
	}

	//fRenegotiateButton->SetEnabled(!disabled);
	fToggleButton->SetLabel(disabled ? "Enable" : "Disable");

	return B_OK;
}
