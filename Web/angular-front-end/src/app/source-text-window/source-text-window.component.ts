import {Component, ElementRef, OnInit, Renderer2, Output, EventEmitter, NgZone, Inject} from '@angular/core';
import {DataService} from '../services/data.service';
import {QueryList, ViewChild, ViewChildren} from '@angular/core';
import {MatButton} from '@angular/material/button';
import {MatCheckbox} from '@angular/material/checkbox';
import {MatMenu, MatMenuTrigger} from '@angular/material/menu';
import {ChangeDetectorRef} from '@angular/core';
import {WordInfoDataSource} from "../word-info.datasource";
import {NestedTreeControl} from '@angular/cdk/tree';
import {TimelineNode} from '../interfaces/timeline-node';
import {TimelineDataSource} from "../timeline.datasource";
import {SourceElement} from "../interfaces/source-element"
import {UntypedFormControl} from "@angular/forms";
import {TemplateRef} from '@angular/core';
import {MatDialog, MatDialogRef, MAT_DIALOG_DATA} from '@angular/material/dialog';
import {
  SourceElementDialogComponent
} from '../source-element-dialog/source-element-dialog.component';
import {SseService} from "../services/sse.service";
import {Router} from "@angular/router";
import { SourceElementTypes } from "../source-element-types"

export interface DialogData {
  animal: string;
  name: string;
}

@Component({
  selector: 'app-source-text-window',
  templateUrl: './source-text-window.component.html',
  styleUrls: ['./source-text-window.component.css']
})
export class SourceTextWindowComponent implements OnInit {
  public displayedColumns: string[] = ['word', 'roleVerbClass', 'relations', 'objectInfo', 'matchingObjects', 'flags'];
  public dataSource: WordInfoDataSource;
  public timelineDataSource: TimelineDataSource;
  //public searchValue: string = "";
  public currentSourceRange: string = "";
  public sourceElements: SourceElement[] = [];
  private preferencesButtonText = 'Preferences';
  public chaptersButtonText = 'Chapters';
  public currentChapter: string = '';
  public chapters = [];
  public agentsButtonText = 'Agents';
  public currentAgent: string = '';
  public agents = [];
  public wordInfo: string = "";
  public roleInfo: string = "";
  public relationsInfo: string = "";
  public toolTip: string = "";

  public searchControl = new UntypedFormControl();
  public sourceSearchString: string = "";
  public searchResults: Map<number, string> = new Map();
  public searchSourceSelected: string = "";

  private timeOutLoadWordInfoId: any;
  private timeoutResizeObserverId: any;
  private timeoutSourceRangeId: any;
  private timeoutInfo: any;
  private timeoutScrollEventListener: any;
  private timeoutSourceSearchStringId: any;

  private loadingSource = true;
  private loadingElements = false;
  private lastFromWhere: string = "0.0";
  private lastToWhere: string = "0.0";
  private bottomReached: boolean = false;
  private selectedPreferences: string[] = [];

  private lastSelectionStartId: number = 0;
  private lastSelectionEndId: number = 0;
  private lastLoadedSource: any;

  //private tempPauseScroll: boolean = false;

  public isHidden = false;
  public preferencesList = [
    {title: 'time', activated: false, value: 0},
    {title: 'space', activated: false, value: 1},
    {title: 'story', activated: false, value: 2},
    {title: 'quote', activated: false, value: 3},
    {title: 'non-time transition', activated: false, value: 4},
    {title: 'all other', activated: false, value: 5},
  ];

  timelineTreeControl = new NestedTreeControl<TimelineNode>(node => node.children);
  hasChild = (_: number, node: TimelineNode) => !!node.children && node.children.length > 0;

  @ViewChild('sourceElementsId') sourceElementsRef!: ElementRef;
  @ViewChild('matMenuPreferencesTrigger', {read: MatMenuTrigger})
  private matMenuPreferencesTriggerRef!: MatMenuTrigger;
  @ViewChild('matMenuChaptersTrigger', {read: MatMenuTrigger})
  private matMenuChaptersTriggerRef!: MatMenuTrigger;
  @ViewChild('matMenuChaptersTrigger', {read: MatButton})
  private matButtonRef!: MatButton;
  @ViewChildren('menuItems')
  private menuItemsRef!: QueryList<MatCheckbox>;
  @ViewChild('amenu', {static: true}) amenu!: MatMenu;
  @ViewChild('sourceSearchStringInput') sourceSearchStringInput!: ElementRef;
  @ViewChild('dialogRef')
  dialogRef!: TemplateRef<any>;
  animal: string = "";
  name: string = "";


  @Output() onSelectedSearchString = new EventEmitter();

  constructor(
    public dataService: DataService,
    private changeDetection: ChangeDetectorRef,
    private _renderer2: Renderer2,
    public sourceElementDialogComponent: MatDialog,
    public dialog: MatDialog,
    private ngZone: NgZone,
    private sseService: SseService,
    private router: Router
  ) {
    this.dataService.login().subscribe(ch => {
      console.log("LOGGED IN!")
      this.sseService
        .getServerSentEvent("http://localhost:5000/stream-get-mode-switch?suid=1&mode_info=1")
        .subscribe(data => console.log("stream-get-mode-switch", data));
    });
    this.dataSource = new WordInfoDataSource(this.dataService);
    this.timelineDataSource = new TimelineDataSource(this.dataService);
  }

  getElementId(tn: Node): number {
    if (tn.nodeType == Node.TEXT_NODE) {
      let parentNode = tn.parentNode
      if (parentNode != null && parentNode.nodeType == Node.ELEMENT_NODE) {
        let parentElement: Element = parentNode as Element;
        return parseInt(parentElement.id);
      }
    }
    return -1;
  }

  ngOnInit() {
    /*
    document.addEventListener("selectstart", function() {
      console.log('Selection started');
    }, false);
     */
    document.addEventListener('selectionchange', () => {
      let selection = document.getSelection();
      if (selection == null || selection.rangeCount == 0)
        return;
      let range = selection.getRangeAt(0);
      let startId = this.getElementId(range.startContainer);
      let endId = this.getElementId(range.endContainer);
      if (startId < endId && (this.lastSelectionStartId != startId || this.lastSelectionEndId != endId)) {
        this.lastSelectionStartId = startId;
        this.lastSelectionEndId = endId;
        clearTimeout(this.timeOutLoadWordInfoId);

        this.timeOutLoadWordInfoId = setTimeout(() => {
          this.dataSource.loadWordInfo(startId, endId, 0, 10);
        }, 300);
      }
    });
    this.searchControl.valueChanges.subscribe(userInput => {
      this.populateSearchStrings(userInput);
    })
  }

  private populateSearchStrings(input: string) {
    this.sourceSearchString = input;
    clearTimeout(this.timeoutSourceSearchStringId);
    this.timeoutSourceSearchStringId = setTimeout(() => {
      this.dataService.getSearchStringsList(this.sourceSearchString).subscribe(strings => {
        this.searchResults = strings
      });
    }, 100);
  }

  // after you clicked an option, this function will show the field you want to show in input
  selectValueSearchSource(selected: any) {
    return selected;
  }

  timedBlinker(elementId: string, interval: number, times: number) {
    if ((times & 1) == 0)
      times += 1
    const el = document.getElementById(elementId);
    if (el != null) {
      let saveStyle = el.style;
      this.timedBlinkerHelper(el, interval, times, saveStyle.backgroundColor, saveStyle.color, saveStyle.fontWeight,
        'black', 'red', 'bold')
    }
  }

  timedBlinkerHelper(el: Element, interval: number, times: number,
                     saveBackColor: string, saveColor: string, saveWeight: string,
                     backColor: string, color: string, weight: string) {
    this._renderer2.setStyle(el, 'background-color', backColor);
    this._renderer2.setStyle(el, 'color', color);
    this._renderer2.setStyle(el, 'font-weight', weight);
    if (times == 0)
      return;
    this.timeoutResizeObserverId = setTimeout(() => {
      this.timedBlinkerHelper(el, interval, times - 1, backColor, color, weight, saveBackColor, saveColor, saveWeight);
    }, interval);
  }

  searchSourceEvent(event: any, result: any) {
    this.dataService.findParagraphStart(result.key.toString() + ".0").subscribe(paragraphStartId => {
      this.scrollToMyRef(paragraphStartId, result.key.toString() + ".0");
      this.sourceSearchStringInput.nativeElement.focus();
      this.sourceSearchStringInput.nativeElement.value = event.source.value;
      this.onSelectedSearchString.emit(this.sourceSearchString)
    });
  }

  isElementVisible(element: Element, sourceElementsTop: number, sourceElementsBottom: number): number {
    let position = element.getBoundingClientRect();

    // checking whether fully visible
    if (position.top >= sourceElementsTop && position.bottom <= sourceElementsBottom) {
      return 2;
    }
    // could check for partial visibility but position.top == position.bottom (???)
    return 0
  }

  findLowestVisibleTextElement(): string {
    let parentElementRect = this.sourceElementsRef.nativeElement.getBoundingClientRect();
    let sourceElementsBottom = parentElementRect.bottom;
    let sourceElementsTop = parentElementRect.top;
    for (let se of this.sourceElements) {
      let e = document.getElementById(se.mouseover.toString())
      if (e != null) {
        if (this.isElementVisible(e, sourceElementsTop, sourceElementsBottom) != 0) {
          // let position = e.getBoundingClientRect();
          return se.mouseover;
        }
      }
    }
    console.log("Unable to find lowest visible element!");
    return "0.0";
  }

  findHighestVisibleTextElement(): string {
    let parentElementRect = this.sourceElementsRef.nativeElement.getBoundingClientRect();
    let sourceElementsBottom = parentElementRect.bottom;
    let sourceElementsTop = parentElementRect.top;
    for (let I = this.sourceElements.length - 1; I >= 0; I--) {
      let se = this.sourceElements[I]
      let e = document.getElementById(se.mouseover.toString())
      if (e != null) {
        if (this.isElementVisible(e, sourceElementsTop, sourceElementsBottom) != 0) {
          return se.mouseover;
        }
      }
    }
    console.log("Unable to find highest visible element!");
    return "0.0";
  }

  triggerExtendBottomScroll(): boolean {
    let parentElementRect = this.sourceElementsRef.nativeElement.getBoundingClientRect();
    let sourceElementsBottom = parentElementRect.bottom;
    let sourceElementsTop = parentElementRect.top;
    let I = this.sourceElements.length - 1;
    let se = this.sourceElements[I]
    let e = document.getElementById(se.mouseover.toString())
    return (e != null && this.isElementVisible(e, sourceElementsTop, sourceElementsBottom) != 0);
  }

  triggerExtendTopScroll(): boolean {
    let parentElementRect = this.sourceElementsRef.nativeElement.getBoundingClientRect();
    let sourceElementsBottom = parentElementRect.bottom;
    let sourceElementsTop = parentElementRect.top;
    let se = this.sourceElements[0]
    let e = document.getElementById(se.mouseover.toString())
    return (e != null && this.isElementVisible(e, sourceElementsTop, sourceElementsBottom) != 0);
  }

  onAgentMenuDisplay() {
    const el = document.getElementById(this.amenu.panelId);
    console.log(this.amenu.panelId);
    console.log(el);
    if (el != null) {
      this._renderer2.setStyle(el, 'min-width', '700px');
      this._renderer2.setStyle(el, 'max-height', '300px');
      this._renderer2.setStyle(el, 'overlay', 'auto');

    }
  }

  updateSourceRange() {
    clearTimeout(this.timeoutSourceRangeId);
    this.timeoutSourceRangeId = setTimeout(() => {
      let lowestElementId = this.findLowestVisibleTextElement();
      let highestElementId = this.findHighestVisibleTextElement();
      this.currentSourceRange = lowestElementId + " - " + highestElementId;
    }, 200);
  }

  changeCursorToWait() {
    console.log("Added waiting to body");
    document.body.classList.add('waiting');   // set cursor to hourglass
  }

  removeCursorWait() {
    document.body.classList.remove('waiting');   // unset cursor hourglass
  }

  ngAfterViewInit() {
    let obs = new ResizeObserver(entries => {
      clearTimeout(this.timeoutResizeObserverId);
      this.timeoutResizeObserverId = setTimeout(() => {
        this.loadElements(1, this.lastFromWhere, null, null);
      }, 300);
    });
    obs.observe(this.sourceElementsRef.nativeElement);
    let scroll = document.querySelector(".source-element");
    if (scroll != null)
      scroll.addEventListener('scroll', (event) => {
        clearTimeout(this.timeoutScrollEventListener);
        this.timeoutScrollEventListener = setTimeout(() => {
          if (this.triggerExtendBottomScroll() && !this.bottomReached) {
            console.log("Bottom Extend");
            this.appendElements(500);
          } else if (this.triggerExtendTopScroll()) {
            let fromWhere = parseInt(this.lastFromWhere.split(".")[0]);
            let newFromWhere = Math.max(fromWhere - 300, 0);
            let scrollToWhere = parseInt(this.findHighestVisibleTextElement().split(".")[0]) - 100;
            if (newFromWhere < fromWhere) {
              console.log("Top extend to " + newFromWhere.toString() + " then bottom scroll to " + scrollToWhere.toString());
              this.loadElements(2, newFromWhere.toString() + ".0", scrollToWhere.toString() + ".0", null);
            }
          } else
            this.updateSourceRange();
        }, 500);
      }, {
        passive: true
      });
    else
      console.log('source-element not found - scroll monitoring not available!')
  }

  public onPreferencesChange() {
    this.selectedPreferences = this.preferencesList
      .filter(menuitem => menuitem.activated)
      .map(menuitem => menuitem.title);
    for (let menuitem of this.preferencesList)
      this.dataService.setPreference(menuitem.value, menuitem.activated).subscribe();
    this.dataService.generateSourceElements().subscribe(ch => {
      console.log(ch['response']);
      console.log("loading elements");
      this.loadElements(3, "0.0", null, null);
    });

  }

  public onPreferencesMenuKeyDown(event: KeyboardEvent, index: number) {
    switch (event.key) {
      case 'ArrowUp':
        if (index > 0) {
          this.setPreferencesCheckboxFocus(index - 1);
        } else {
          this.menuItemsRef.last.focus();
        }
        break;
      case 'ArrowDown':
        if (index !== this.menuItemsRef.length - 1) {
          this.setPreferencesCheckboxFocus(index + 1);
        } else {
          this.setPreferencesFocusOnFirstItem();
        }
        break;
      case 'Enter':
        event.preventDefault();
        this.preferencesList[index].activated
          = !this.preferencesList[index].activated;
        this.onPreferencesChange();
        setTimeout(() => this.matMenuPreferencesTriggerRef.closeMenu(), 200);
        break;
    }
  }

  public onPreferencesMenuOpened() {
    this.setPreferencesFocusOnFirstItem();
    this.preferencesButtonText = 'Close menu';
  }

  public onPreferencesMenuClosed() {
    this.matButtonRef.focus();
    if (this.selectedPreferences.length > 0)
      this.preferencesButtonText = this.selectedPreferences.join(', ');
    else
      this.preferencesButtonText = 'Preferences';
  }

  private setPreferencesFocusOnFirstItem(): void {
    const firstCheckbox = this.menuItemsRef.first;
    firstCheckbox.focus();
    firstCheckbox._elementRef
      .nativeElement.classList.add('cdk-keyboard-focused');
  }

  private setPreferencesCheckboxFocus(index: number) {
    const checkBox = this.menuItemsRef.get(index);
    if (checkBox) {
      checkBox.focus();
    }
  }

  scrollToMyRef = (id: string, strobe: string | null) => {
    let ref = document.getElementById(id);
    if (ref == null) {
      console.log("Cannot find element id " + id)
      this.loadElements(4, id, null, strobe);
      this.isHidden = false;
    } else {
      setTimeout(() => {
        console.log("scrolling to " + id.toString());
        let ref = document.getElementById(id);
        if (ref != null) {
          ref.scrollIntoView({
            behavior: "auto",
            block: "start",
          });
          this.isHidden = false;
          this.changeDetection.detectChanges();
          if (strobe != null)
            this.timedBlinker(strobe, 300, 10);
        } else {
          console.log("element id = " + id.toString() + " not found!");
        }
      }, 50);
    }
  };

  public onChaptersSelect(chapter: any) {
    console.log(chapter);
    this.scrollToMyRef(chapter['where'], null);
  }

  public onAgentsSelect(item: any) {
    //this.scrollToMyRef(item['position']);
  }


  onSelectedOption(e: any) {
    this.lastLoadedSource = e;
    this.loadSource(e);
  }

  reload() {
    this.loadSource(this.lastLoadedSource);
  }

  openDialog(): void {
    const dialogRef = this.dialog.open(DialogOverviewExampleDialog, {
      width: '250px',
      data: {name: this.name, animal: this.animal},
    });

    dialogRef.afterClosed().subscribe(result => {
      console.log('The dialog was closed');
      this.animal = result;
    });
  }


  // this click is done in a non-angular element.  Therefore sourceElementClick is outside the
  // Angular Zone and must be put back in, otherwise components will not display or initialize correctly.
  sourceElementClick(elementId: string) {
    this.ngZone.run(() => {
      //console.log("sourceElementClick In Angular Zone?");
      //console.log(NgZone.isInAngularZone());
      //console.log(elementId);
      this.dataService.getElementType(elementId).subscribe(elementType => {
        let sourceElementDialogComponentRef: any;
        console.log("element id " + elementId + " has element type " + elementType.toString());
        switch (elementType) {
          case SourceElementTypes.wordType:
          case SourceElementTypes.matchingSpeakerType:
          case SourceElementTypes.matchingType:
          case SourceElementTypes.audienceMatchingType:
          case SourceElementTypes.matchingObjectType:
            sourceElementDialogComponentRef = this.sourceElementDialogComponent.open(SourceElementDialogComponent,
              {
                data: {
                  'dataService': this.dataService,
                  'matchingType': elementType,
                  "elementId": elementId
                }
              });
            break;
          default:
            console.log("elementType doesn't match.");
        }
      });
    });
  }

  loadSource(e: any) {
    console.log("loading source");
    this.loadingSource = true
    this.changeCursorToWait();
    this.dataService.loadSource(e).subscribe(elements => {
      this.loadingSource = false
      this.loadElements(5, "0.0", null, null);
      this.dataService.loadChapters().subscribe(ch => {
        this.chapters = ch['response']
      });
      this.dataService.loadAgents().subscribe(ch => {
        this.agents = ch['response']
      });
      this.timelineDataSource.loadTimelines(0, 100);
    });
  }

  loadElements(caller: number, fromWhere: string, optionScrollTo: string | null, strobe: string | null) {
    if (this.loadingSource)
      return;
    if (this.loadingElements)
      return;
    let width = this.sourceElementsRef.nativeElement.offsetWidth;
    let height = this.sourceElementsRef.nativeElement.offsetHeight;
    // console.log("before", caller, width, height, this.sourceElements.length);
    if (width == 0 || height == 0)
      return;
    this.loadingElements = true;
    this.dataService.loadElements(width, height, fromWhere).subscribe(elements => {
      if (optionScrollTo != null)
        this.isHidden = true;
      if (elements['response'].length > 0) {
        this.sourceElements = elements['response'];
        this.lastFromWhere = fromWhere;
        // console.log("after", caller, width, height, elements['response'].length, this.sourceElements[0]['mouseover'], this.sourceElements[this.sourceElements.length - 1]['mouseover']);
        this.lastToWhere = this.sourceElements[this.sourceElements.length - 1]['mouseover'];
        this.loadingElements = false;
        this.isHidden = false;
        this.removeCursorWait();
        if (optionScrollTo != null)
          this.timeoutInfo = setTimeout(() => {
            console.log("scroll to " + optionScrollTo);
            let ref = document.getElementById(optionScrollTo);
            if (ref != null) {
              ref.scrollIntoView({
                behavior: "auto",
                block: "end",
              });
              if (strobe != null)
                this.timedBlinker(strobe, 300, 10);
            } else {
              console.log("element id = " + optionScrollTo + " not found!");
            }
            this.changeDetection.detectChanges();
          }, 25);
        else {
          this.changeDetection.detectChanges();
          if (strobe != null)
            this.timedBlinker(strobe, 300, 10);
        }

      }
      this.updateSourceRange();
    });
  }

  appendElements(numElements: number) {
    if (this.loadingSource)
      return;
    if (this.loadingElements)
      return;
    this.loadingElements = true;
    let width = this.sourceElementsRef.nativeElement.offsetWidth;
    let height = this.sourceElementsRef.nativeElement.offsetHeight;
    let lastToWhere = this.sourceElements[this.sourceElements.length - 1]['mouseover'];
    lastToWhere = (parseInt(lastToWhere.split(".")[0]) + 1).toString() + ".0";
    this.dataService.loadElements(width, height, lastToWhere).subscribe(elements => {
      this.sourceElements = this.sourceElements.concat(elements['response']);
      this.loadingElements = false;
      this.changeDetection.detectChanges();
      this.updateSourceRange();
    });
  }

  displayElement(e: any) {
    clearTimeout(this.timeoutInfo);
    this.timeoutInfo = setTimeout(() => {
      this.dataService.loadInfoPanel(e).subscribe(ch => {
        this.wordInfo = ch['response']["wordInfo"];
        this.roleInfo = ch['response']["roleInfo"];
        this.relationsInfo = ch['response']["relationsInfo"];
        this.toolTip = ch['response']["toolTip"];
        this.changeDetection.detectChanges();
      });
    }, 200);
    return e;
  }

  /*
  openContentElement() {
    this.dialog.open(ContentElementDialog);
  }
*/
}

@Component({
  selector: 'dialog-overview-example-dialog',
  templateUrl: '../dialog-overview-example-dialog.html',
})
export class DialogOverviewExampleDialog {
  constructor(
    public dialogRef: MatDialogRef<DialogOverviewExampleDialog>,
    @Inject(MAT_DIALOG_DATA) public data: DialogData,
  ) {
  }

  onNoClick(): void {
    this.dialogRef.close();
  }
}
