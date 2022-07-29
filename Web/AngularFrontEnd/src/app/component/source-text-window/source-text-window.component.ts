import { Component, ElementRef, OnInit, HostListener, Renderer2 } from '@angular/core';
import { DataService } from '../data.service';
import { QueryList, ViewChild, ViewChildren, ViewEncapsulation} from '@angular/core';
import { MatButton } from '@angular/material/button';
import { MatCheckbox } from '@angular/material/checkbox';
import {MatMenu, MatMenuTrigger} from '@angular/material/menu';
import { ChangeDetectorRef } from '@angular/core';
import { WordInfoDataSource } from "../word-info.datasource";
import {NestedTreeControl} from '@angular/cdk/tree';
import {TimelineNode} from '../timelineNode';
import {TimelineDataSource} from "../timeline.datasource";
import {SourceElement} from "../source-element"

@Component({
  selector: 'app-source-text-window',
  templateUrl: './source-text-window.component.html',
  styleUrls: ['./source-text-window.component.css']
})

export class SourceTextWindowComponent implements OnInit {
  public displayedColumns: string[] = ['word', 'roleVerbClass', 'relations', 'objectInfo', 'matchingObjects', 'flags'];
  public dataSource: WordInfoDataSource;
  public timelineDataSource : TimelineDataSource;
  public searchValue: string = "";
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

  private timeOutLoadWordInfoId: any;
  private timeoutResizeObserverId: any;
  private timeoutSourceRangeId: any;

  private loadingSource = true
  private lastFromWhere: number = 0;
  private lastToWhere: number = 0;
  private selectedPreferences: string[] = [];

  private lastSelectionStartId: number = 0;
  private lastSelectionEndId: number = 0;

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

  constructor(
    private dataService: DataService,
    private changeDetection: ChangeDetectorRef,
    private _renderer2: Renderer2
  ) {
    this.dataService.login().subscribe(ch => {
      console.log("LOGGED IN!")
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
      if (selection == null)
        return;
      let range = selection.getRangeAt(0);
      let startId = this.getElementId(range.startContainer);
      let endId = this.getElementId(range.endContainer);
      if (startId < endId && (this.lastSelectionStartId != startId || this.lastSelectionEndId != endId)) {
        console.log('loadWordInfoTimeout started' + startId + '-' + endId);
        this.lastSelectionStartId = startId;
        this.lastSelectionEndId = endId;
        clearTimeout(this.timeOutLoadWordInfoId);

        this.timeOutLoadWordInfoId = setTimeout(() => {
          this.dataSource.loadWordInfo(startId, endId, 0, 10);
        }, 300);
      }
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

  findLowestVisibleTextElement(): number {
    let parentElementRect = this.sourceElementsRef.nativeElement.getBoundingClientRect();
    let sourceElementsBottom = parentElementRect.bottom;
    let sourceElementsTop = parentElementRect.top;
    for (let se of this.sourceElements) {
      let e = document.getElementById(se.mouseover.toString())
      if (e != null) {
        if (this.isElementVisible(e, sourceElementsTop, sourceElementsBottom) != 0) {
          let position = e.getBoundingClientRect();
          console.log("lowest element index visible: " + se.mouseover);
          return se.mouseover;
        }
      }
    }
    console.log("Unable to find lowest visible element!");
    return -1;
  }

  findHighestVisibleTextElement(): number {
    let parentElementRect = this.sourceElementsRef.nativeElement.getBoundingClientRect();
    let sourceElementsBottom = parentElementRect.bottom;
    let sourceElementsTop = parentElementRect.top;
    for (let I = this.sourceElements.length - 1; I >= 0; I--) {
      let se = this.sourceElements[I]
      let e = document.getElementById(se.mouseover.toString())
      if (e != null) {
        if (this.isElementVisible(e, sourceElementsTop, sourceElementsBottom) != 0) {
          console.log("highest element index visible: " + se.mouseover);

          return se.mouseover;
        }
      }
    }
    console.log("Unable to find highest visible element!");
    return -1;
  }

  triggerRetrieveMoreFromScroll(): boolean {
    let parentElementRect = this.sourceElementsRef.nativeElement.getBoundingClientRect();
    let sourceElementsBottom = parentElementRect.bottom;
    let sourceElementsTop = parentElementRect.top;
    let I = this.sourceElements.length - 1;
    let se = this.sourceElements[I]
    let e = document.getElementById(se.mouseover.toString())
    return (e != null && this.isElementVisible(e, sourceElementsTop, sourceElementsBottom) != 0);
  }

  onAgentMenuDisplay()
  {
    const el = document.getElementById(this.amenu.panelId);
    console.log(this.amenu.panelId);
    console.log(el);
    if (el!=null)
    {
      this._renderer2.setStyle(el, 'min-width', '700px');
      this._renderer2.setStyle(el, 'max-height', '300px');
      this._renderer2.setStyle(el, 'overlay', 'auto');

    }
  }

  updateSourceRange()
  {
    clearTimeout(this.timeoutSourceRangeId);
    this.timeoutSourceRangeId = setTimeout(() => {
      let lowestElementId = this.findLowestVisibleTextElement();
      let highestElementId = this.findHighestVisibleTextElement();
      this.dataService.HTMLElementIdToSource(lowestElementId).subscribe(lowestSourcePosition => {
        this.dataService.HTMLElementIdToSource(highestElementId).subscribe(highestSourcePosition => {
          this.currentSourceRange = lowestSourcePosition.toString() + " - " + highestSourcePosition.toString();
        });
      });
    }, 300);
  }

  ngAfterViewInit() {
    let obs = new ResizeObserver(entries => {
      clearTimeout(this.timeoutResizeObserverId);
      this.timeoutResizeObserverId = setTimeout(() => { this.loadElements(this.lastFromWhere); }, 300);
    });
    obs.observe(this.sourceElementsRef.nativeElement);
    let scroll = document.querySelector(".source-element");
    if (scroll!=null)
      scroll.addEventListener('scroll', (event) => {
        this.updateSourceRange();
        if (this.triggerRetrieveMoreFromScroll())
        {

        }
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
      this.loadElements(0);
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
    if (this.selectedPreferences.length>0)
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

  scrollToMyRef = (id:number) => {
    let ref = document.getElementById(id.toString());
    if (ref == null)
      this.loadElements(id);
    else {
      setTimeout(function () {
        if (ref != null) {
          ref.scrollIntoView({
            behavior: "smooth",
            block: "start",
          });
        } else {
          console.log("element id = " + id.toString() + " not found!");
          console.log(id);
        }
      }, 300);
    }
  };

  public onChaptersSelect(item:any) {
    this.scrollToMyRef(item['position']);
  }

  public onAgentsSelect(item:any) {
    //this.scrollToMyRef(item['position']);
  }


  onSelectedOption(e:any) {
    this.loadSource(e);
  }

  loadSource(e:any)
  {
    console.log("loading source");
    this.loadingSource=true
    this.dataService.loadSource(e).subscribe(elements => {
      //console.log(elements['response']);
      this.sourceElements = elements['response']
      this.dataService.generateSourceElements().subscribe(ch => {
        this.loadingSource=false
        //console.log(ch['response']);
        //console.log("loading elements");
        this.loadElements(0);
        this.dataService.loadChapters().subscribe(ch => {
          //console.log(ch['response']);
          this.chapters = ch['response']
        });
        this.dataService.loadAgents().subscribe(ch => {
          console.log('AGENTS');
          console.log(ch['response']);
          this.agents = ch['response']
        });
        this.timelineDataSource.loadTimelines(0, 100);
      });
    });


  }

  loadElements(fromWhere: number)
  {
    if (this.loadingSource)
      return;
    let width = this.sourceElementsRef.nativeElement.offsetWidth;
    let height = this.sourceElementsRef.nativeElement.offsetHeight;
    this.dataService.loadElements(width, height, fromWhere).subscribe(elements => {
      //console.log(elements);
      this.sourceElements = elements['response'];
      this.lastFromWhere = fromWhere;
      this.lastToWhere = this.sourceElements[this.sourceElements.length-1]['mouseover'];
      this.changeDetection.detectChanges();
      this.updateSourceRange();
    });
  }

  displayElement(e: any)
  {
    this.dataService.loadInfoPanel(e).subscribe(ch => {
       this.wordInfo = ch['response']["wordInfo"];
       this.roleInfo = ch['response']["roleInfo"];
       this.relationsInfo = ch['response']["relationsInfo"];
       this.toolTip = ch['response']["toolTip"];
      this.changeDetection.detectChanges();
    });
    return e;
  }

}
