import bb.cascades 1.0
import mopubview.lib 1.0

Page {
    id: pageId
    attachedObjects: [
        Sheet {
            id: intersitialSheet
            content: Page {
                Container {
                    id: intersitialAdContainer
                    layout: DockLayout {
                    }
                    MoPubView{
                        id: intersitialAd
                        horizontalAlignment: HorizontalAlignment.Center
                        verticalAlignment: VerticalAlignment.Center
                        adUnitId:"agltb3B1Yi1pbmNyDAsSBFNpdGUYsckMDA"
                    }
                    Button {
                        id: sheetbutton
                        horizontalAlignment: HorizontalAlignment.Center
                        verticalAlignment: VerticalAlignment.Bottom
                        text: "Close"
                        onClicked: intersitialSheet.close()
                    }
                }
            }
            onOpened: intersitialAd.loadAd()
        }
    ]
    titleBar: TitleBar {
        title: "MoPub Simple Ad demo"
    }
    Container {
        id: rootContainer
        onCreationCompleted: {
            bannerAdId.loadAd();
            squareAdId.loadAd();
        }
        Container {
            id: bannerBorder
            horizontalAlignment: HorizontalAlignment.Fill
            leftPadding: 2.0
            rightPadding: 2.0
            topPadding: 2.0
            bottomPadding: 2.0
            background: Color.create(.4,.4,.4) 
            MoPubView {
                id: bannerAdId
                horizontalAlignment: HorizontalAlignment.Center
                width: 320
                height: 50
                adUnitId: "agltb3B1Yi1pbmNyDAsSBFNpdGUYkaoMDA"
            }
        }
        Container {
            id: rectAdBorder
            horizontalAlignment: HorizontalAlignment.Fill
            leftPadding: 2.0
            rightPadding: 2.0
            topPadding: 2.0
            bottomPadding: 2.0
            background: Color.create(.2,.2,.2)    
            MoPubView {
                id: squareAdId
                horizontalAlignment: HorizontalAlignment.Center
                width: 300
                height: 250
                adUnitId: "agltb3B1Yi1pbmNyDAsSBFNpdGUYycEMDA"
            }
        }

        Container {
            id: filler
            layoutProperties: StackLayoutProperties {spaceQuota: 1}
            verticalAlignment: VerticalAlignment.Fill
            horizontalAlignment: HorizontalAlignment.Fill
            background: Color.Gray            
            TextArea {
                objectName: consoleLog
                input.submitKey: SubmitKey.None
                editable: false
                textStyle.color: Color.White
                backgroundVisible: false
                text: bannerAdId.adHtml
                textStyle.fontSizeValue: 5.0
            }
        }
        Container {
            id: buttonsContainer
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }
            Button {
                id: bannerButton
                text: "Banners"
                onClicked: {
                    squareAdId.loadAd();
                    bannerAdId.loadAd();
                }
                layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
            }
            Button {
                id: sheetButton
                text: "Intersitial"
                onClicked: intersitialSheet.open()
                layoutProperties: StackLayoutProperties { spaceQuota: 1.0 }
            }
        }
    }
}
