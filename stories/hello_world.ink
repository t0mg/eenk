VAR player_name = "Stranger"

-> start

=== start ===
The ancient e-ink screen flickers to life.
A single line of text materializes, letter by letter.

"Welcome, {player_name}. You stand at a crossroads."

* [Head north into the forest]
    -> forest
* [Follow the river east]
    -> river
* [Examine the strange device in your hands]
    -> examine_device

=== forest ===
The trees close in around you. Dappled light filters through leaves that haven't changed in centuries.

A path splits ahead.

* [Take the left fork]
    The left fork leads to a clearing. In the center, an old stone well.
    You peer inside. Darkness stares back.
    -> well
* [Take the right fork]
    The right fork is overgrown. You push through brambles until you reach a moss-covered ruin.
    -> ruins
* [Turn back]
    -> start

=== river ===
The river runs clear and cold. Smooth stones line the banks.

Something glints beneath the surface.

* [Reach into the water]
    ~ player_name = "River-Touched"
    Your fingers close around a small brass key. As you lift it, the water whispers your new name.
    You are now {player_name}.
    -> key_found
* [Follow the river downstream]
    You walk for what feels like hours. The river widens into a delta.
    In the distance, a lighthouse.
    -> lighthouse
* [Turn back]
    -> start

=== examine_device ===
You turn the device over in your hands. It's impossibly thin.
A screen of electronic paper. Six buttons. No touchscreen.
It feels like holding a piece of the future.

"This is your story," the screen reads. "Every choice matters."

* [Press the confirm button]
    The screen clears. A new chapter begins.
    -> start
* [Put the device away]
    -> END

=== well ===
The well is deep. Very deep.

* [Drop a stone]
    You wait. And wait. No splash ever comes.
    "Some depths have no bottom," the screen observes.
    -> END
* [Walk away]
    -> forest

=== ruins ===
The ruins whisper of a civilization that read only on paper.
A single book remains on a crumbling shelf.

* [Open the book]
    The pages are blank. But as you watch, text appears:
    "THE END IS ALSO A BEGINNING."
    -> END
* [Leave the book]
    Some stories are best left unread.
    -> END

=== key_found ===
The brass key feels warm in your palm. But there is no lock in sight.

* [Keep the key]
    You pocket the key. Perhaps its purpose will reveal itself.
    "Not all keys open doors," {player_name}.
    -> END
* [Throw the key back]
    The river accepts the key silently. The glinting stops.
    -> river

=== lighthouse ===
The lighthouse stands alone against the grey sky.
Its light has been dark for years.

* [Climb to the top]
    From the top, you can see the entire world—forest, river, ruins, and beyond.
    The e-ink screen renders it all in perfect monochrome.
    "Every ending is a panorama," {player_name}.
    -> END
* [Enter the keeper's quarters]
    A desk. A chair. A journal.
    The last entry reads: "The reader has arrived."
    -> END
