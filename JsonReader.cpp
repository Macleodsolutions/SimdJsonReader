#include "JsonReader.h"

JsonReader::JsonReader(QWidget* parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	ui.treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
}

JsonReader::~JsonReader() {}

// Method: Triggered when the "Load" button is clicked. Opens a file dialog to select a JSON file
void JsonReader::on_loadBtn_clicked() {

	// Open a dialog for the user to select a JSON file
	QString filename = QFileDialog::getOpenFileName(
		this,
		"Open JSON file",
		"",
		"JSON Files (*.json);;All Files (*)"
	);

	// If the user cancelled the dialog, terminate the method
	if (filename.isEmpty()) {
		return;
	}

	// Parse the selected JSON file
	simdjson::dom::element doc;
	auto error = parser.load(filename.toStdString()).get(doc);
	if (error) {
		qInfo() << "Error: " << error;
		return;
	}

	// Add parsed JSON data to the tree widget
	QTreeWidgetItem* root = new QTreeWidgetItem();
	add_children_to_item(root, doc);
	ui.treeWidget->insertTopLevelItem(0, root);
}

// Method: Triggered when the "Copy" button is clicked. Copies selected items to the clipboard
void JsonReader::on_copyBtn_clicked() {

	// Get a list of all currently selected items
	QList<QTreeWidgetItem*> selectedItems = ui.treeWidget->selectedItems();
	QStringList pairs;

	// Iterate over selected items and add their data to 'pairs'
	for (const auto& item : selectedItems) {
		QStringList itemPairs = copy_recursive(item);
		for (const QString& itemPair : itemPairs) {
			pairs.append(itemPair);
		}
	}

	// Copy 'pairs' to the clipboard
	QApplication::clipboard()->setText(pairs.join(", "));
}

// Method: Triggered when a tree widget item is expanded. Updates the tree widget to show the item's children
void JsonReader::on_treeWidget_itemExpanded(QTreeWidgetItem* item) {

	// Check if the expanded item is in 'itemElementMap'
	if (itemElementMap.contains(item)) {

		// If the item only has a single placeholder child, remove it
		if (item->childCount() == 1 && item->child(0)->text(0) == "") {
			delete item->takeChild(0);
		}

		// Add the item's real children to the tree widget
		simdjson::dom::element element = itemElementMap.value(item);
		add_children_to_item(item, element);

		// Remove the item from 'itemElementMap' to prevent it from being parsed again
		itemElementMap.remove(item);
	}
}

// Method: Triggered when the content of 'textEdit' changes. Starts a new search from the current selection
void JsonReader::on_textEdit_textChanged() {

	// Get the search text from 'textEdit'
	QString searchText = ui.textEdit->toPlainText();
	if (searchText.isEmpty()) {
		return;
	}

	// Get the currently selected item in the tree widget
	QTreeWidgetItem* startItem = ui.treeWidget->currentItem();

	// If no item is selected, start the search from the root item
	if (startItem == nullptr) {

		// Start the search from each top-level item
		const int topLevelCount = ui.treeWidget->topLevelItemCount();
		for (int i = 0; i < topLevelCount; ++i) {
			startItem = ui.treeWidget->topLevelItem(i);
			if (this->searchTree(startItem, searchText, false, true)) {
				lastSearchText = searchText;
				return;
			}
		}
	}
	else {

		// If an item is selected, start the search from that item
		if (this->searchTree(startItem, searchText, false, true)) {
			lastSearchText = searchText;
			return;
		}
	}

	// If the search text was not found, log a message
	qInfo() << "Search text not found";
}

// Method: Triggered when the state of 'radioCapital' changes. If there's search text, re-triggers the search
void JsonReader::on_radioCapital_toggled(bool checked) {

	// We don't use the 'checked' parameter
	Q_UNUSED(checked);

	// If there's search text in 'textEdit', re-trigger the search
	QString searchText = ui.textEdit->toPlainText();
	if (!searchText.isEmpty()) {
		on_textEdit_textChanged();
	}
}

// Method: Triggered when the "Search Next" button is clicked. Searches for the next occurrence of the search text
void JsonReader::on_searchNextBtn_clicked() {

	// Get the search text from 'textEdit'
	QString searchText = ui.textEdit->toPlainText();

	// If there's no search text or it has changed since the last search, terminate the method
	if (searchText.isEmpty() || searchText != lastSearchText) {
		return;
	}

	// Determine if we've passed the current match yet
	bool pastCurrentMatch = lastMatch == nullptr;

	// Start the search from each top-level item
	const int topLevelCount = ui.treeWidget->topLevelItemCount();
	for (int i = 0; i < topLevelCount; ++i) {
		QTreeWidgetItem* startItem = ui.treeWidget->topLevelItem(i);

		if (this->searchTree(startItem, searchText, !pastCurrentMatch)) {

			// If we've passed the current match, this is the next match
			if (pastCurrentMatch) {
				return;
			}
			else if (lastMatch == startItem) {

				// If this is the current match, continue searching for the next one
				pastCurrentMatch = true;
			}
		}
	}

	// If there are no further matches, log a message
	qInfo() << "No further matches found";
}
