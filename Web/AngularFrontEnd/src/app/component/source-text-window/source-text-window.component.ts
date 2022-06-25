import { AfterViewInit, Component, ElementRef, OnInit } from '@angular/core';
import { DataService } from '../data.service';
import { Source } from '../source';
import { QueryList, ViewChild, ViewChildren, ViewEncapsulation} from '@angular/core';
import { MatButton } from '@angular/material/button';
import { MatCheckbox } from '@angular/material/checkbox';
import { MatMenuTrigger } from '@angular/material/menu';

@Component({
  selector: 'app-source-text-window',
  templateUrl: './source-text-window.component.html',
  styleUrls: ['./source-text-window.component.css']
})
export class SourceTextWindowComponent implements OnInit {
  selectedName: any;
  selectedType: any;
  bookTitle: string = "Default Book Name";
  searchValue: string = "";
  author:string = "";
  title: string = "";
  sourceElements: [] = [];
  source_in_current_scrolled_window = [
    { name: 'This', _id: 1 },
    { name: 'Old', _id: 2 },
    { name: 'Man', _id: 3 },
    { name: 'Went', _id: 4 },
    { name: 'Around', _id: 5 },
  ];
  public attributesButtonText = 'Attributes';
  public chaptersButtonText = 'Chapters';
  public currentChapter: string = '';

  public attributesList = [
    { title: 'time', activated: false, value: 'time' },
    { title: 'space', activated: false, value: 'space' },
    { title: 'story', activated: false, value: 'story' },
    { title: 'quote', activated: false, value: 'quote' },
    { title: 'non-time transition', activated: false, value: 'ntt' },
    { title: 'all other', activated: false, value: 'other' },
  ];
  public chapters = [];
  private selectedAttributes: string[] = [];

  @ViewChild('sourceElementsId')
  myIdentifier!: ElementRef;
  @ViewChild('matMenuAttributesTrigger', { read: MatMenuTrigger })
  private matMenuAttributesTriggerRef!: MatMenuTrigger;
  @ViewChild('matMenuChaptersTrigger', { read: MatMenuTrigger })
  private matMenuChaptersTriggerRef!: MatMenuTrigger;
  @ViewChild('matMenuChaptersTrigger', { read: MatButton })
  private matButtonRef!: MatButton;
  @ViewChildren('menuItems')
  private menuItemsRef!: QueryList<MatCheckbox>;

  constructor(
    private dataService: DataService
  )
  {

  }

  initialize() {
  }

  ngOnInit() {
    // dynamically sets button text with default value Join
    this.initialize();
  }

  public onAttributesSelect() {
    this.selectedAttributes = this.attributesList
      .filter(menuitem => menuitem.activated)
      .map(menuitem => menuitem.title);
  }

  public onAttributesMenuKeyDown(event: KeyboardEvent, index: number) {
    switch (event.key) {
      case 'ArrowUp':
        if (index > 0) {
          this.setAttributesCheckboxFocus(index - 1);
        } else {
          this.menuItemsRef.last.focus();
        }
        break;
      case 'ArrowDown':
        if (index !== this.menuItemsRef.length - 1) {
          this.setAttributesCheckboxFocus(index + 1);
        } else {
          this.setAttributesFocusOnFirstItem();
        }
        break;
      case 'Enter':
        event.preventDefault();
        this.attributesList[index].activated
          = !this.attributesList[index].activated;
        this.onAttributesSelect();
        setTimeout(() => this.matMenuAttributesTriggerRef.closeMenu(), 200);
        break;
    }
  }

  public onAttributesMenuOpened() {
    this.setAttributesFocusOnFirstItem();
    this.attributesButtonText = 'Close menu';
  }

  public onAttributesMenuClosed() {
    this.matButtonRef.focus();
    if (this.selectedAttributes.length>0)
      this.attributesButtonText = this.selectedAttributes.join(', ');
    else
      this.attributesButtonText = 'Attributes';
  }

  private setAttributesFocusOnFirstItem(): void {
    const firstCheckbox = this.menuItemsRef.first;
    firstCheckbox.focus();
    firstCheckbox._elementRef
      .nativeElement.classList.add('cdk-keyboard-focused');
  }

  private setAttributesCheckboxFocus(index: number) {
    const checkBox = this.menuItemsRef.get(index);
    if (checkBox) {
      checkBox.focus();
    }
  }

  public onChaptersSelect() {
    // load chapter starting at where
  }


  onSelectedOption(e:any) {
    this.loadSource(e);
  }

  loadSource(e:any)
  {
    console.log("loading source");
    this.dataService.loadSource(e).subscribe(elements => {
      console.log(elements['response']);
      this.sourceElements = elements['response']
      console.log("loading elements");
      this.loadElements();
      this.dataService.loadChapters(e).subscribe(ch => {
        console.log(ch['response']);
        this.chapters = ch['response']
      });
    });


  }

  loadElements()
  {
    let width = this.myIdentifier.nativeElement.offsetWidth;
    let height = this.myIdentifier.nativeElement.offsetHeight;
    console.log('Width:' + width);
    console.log('Height: ' + height);
    this.dataService.loadElements(width, height, 0).subscribe(elements => {
      console.log(elements);
      this.sourceElements = elements['response'];
    });
  }

  displayMetaInfo(e: any)
  {

  }

  displayElement(e: any)
  {
    console.log(e);
    return e;
  }
}
