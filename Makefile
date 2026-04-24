CC := gcc

# //CHANGE
.PHONY: build test clean dashboard-install dashboard-dev dashboard-build

build:
	$(MAKE) -C final/recorder all
	$(MAKE) -C final/replayer all
	$(MAKE) -C final/visualizer all

test: build
	node scripts-CHANGE/make-test-CHANGE.mjs

dashboard-install:
	npm --prefix dashboard-CHANGE install

dashboard-dev:
	npm --prefix dashboard-CHANGE run dev

dashboard-build:
	npm --prefix dashboard-CHANGE run build

clean:
	$(MAKE) -C final/recorder clean
	$(MAKE) -C final/replayer clean
	$(MAKE) -C final/visualizer clean
	rm -rf out
# //CHANGE
